#include <Arduino.h>
#include <esp_task_wdt.h>
#include "drivers/EncoderEC12.h"
#include "drivers/EncoderC14.h"
#include "drivers/EMSPulseGenerator.h"
#include "app/pins.h"
#include "app/AppState.h"
#include "app/CommandQueue.h"

// ============================================
// Глобальные объекты
// ============================================
static AppState appState;
static CommandQueue commandQueue(10);
static EncoderEC12 encoderA(ENC_A_CLK_PIN, ENC_A_DT_PIN, 1000);
static EncoderC14  encoderB(ENC_B_CLK_PIN, ENC_B_DT_PIN, 50);
//static EMSPulseGenerator stim;

// 🔥 ДВА НЕЗАВИСИМЫХ ГЕНЕРАТОРА
// Генератор 1: PWM канал 0, пин 1, частота 144 Гц, разрешение 10 бит
EMSPulseGenerator pwm_stim_1(0, PWM_CH_1_PIN, 144, 10, 70);

// Генератор 2: PWM канал 1, пин 2, частота 144 Гц, разрешение 10 бит
EMSPulseGenerator pwm_stim_2(1, PWM_CH_2_PIN, 1245, 10, 110);

// ============================================
// Константы
// ============================================
constexpr int STEPS_PER_REV = 20;
constexpr uint32_t WDT_TIMEOUT_SEC = 5;

// Настройки производительности
constexpr uint32_t UI_TASK_DELAY_MS = 10;
constexpr uint32_t STIM_TASK_DELAY_MS = 0;
constexpr uint32_t STATS_INTERVAL_MS = 10000;

// Размеры стека
constexpr uint32_t UI_TASK_STACK_SIZE = 8192;
constexpr uint32_t STIM_TASK_STACK_SIZE = 8192;

// ============================================
// Task Handles
// ============================================
TaskHandle_t uiTaskHandle = nullptr;
TaskHandle_t stimTaskHandle = nullptr;

 // ============================================
// Статистика
// ============================================
struct TaskStats {
    uint32_t loopCount = 0;
    uint32_t lastPrintTime = 0;
    uint32_t commandsSent = 0;
    uint32_t commandsReceived = 0;
    uint32_t maxLoopTime = 0;
    int8_t coreId = -1;
    float cpuUsage = 0.0f;
    
    // НОВОЕ: Накопительное время выполнения
    uint64_t totalActiveTimeUs = 0;  // Общее активное время в микросекундах
};

static TaskStats uiStats;
static TaskStats stimStats;

// Статистика для расчета загрузки CPU
struct CpuStats {
    uint32_t lastUpdateTime = 0;
    uint64_t lastUiActiveTime = 0;   // ИЗМЕНЕНО: uint64_t
    uint64_t lastStimActiveTime = 0; // ИЗМЕНЕНО: uint64_t
    float core0Usage = 0.0f;
    float core1Usage = 0.0f;
};
static CpuStats cpuStats;

// ============================================
// ИСПРАВЛЕННАЯ функция расчета загрузки CPU
// ============================================
static void updateCpuUsage() {
    uint32_t currentTime = millis();
    uint32_t deltaTime = currentTime - cpuStats.lastUpdateTime;

    if (deltaTime == 0 || cpuStats.lastUpdateTime == 0) {
        cpuStats.lastUpdateTime = currentTime;
        cpuStats.lastUiActiveTime = uiStats.totalActiveTimeUs;
        cpuStats.lastStimActiveTime = stimStats.totalActiveTimeUs;
        return;
    }

    // Рассчитываем активное время за период
    uint64_t uiActiveUs = uiStats.totalActiveTimeUs - cpuStats.lastUiActiveTime;
    uint64_t stimActiveUs = stimStats.totalActiveTimeUs - cpuStats.lastStimActiveTime;

    // Переводим deltaTime в микросекунды
    uint64_t deltaTimeUs = (uint64_t)deltaTime * 1000ULL;

    // Процент загрузки = (активное время / общее время) * 100
    uiStats.cpuUsage = ((float)uiActiveUs / (float)deltaTimeUs) * 100.0f;
    stimStats.cpuUsage = ((float)stimActiveUs / (float)deltaTimeUs) * 100.0f;

    // Ограничиваем значения
    if (uiStats.cpuUsage > 100.0f) uiStats.cpuUsage = 100.0f;
    if (uiStats.cpuUsage < 0.0f) uiStats.cpuUsage = 0.0f;
    if (stimStats.cpuUsage > 100.0f) stimStats.cpuUsage = 100.0f;
    if (stimStats.cpuUsage < 0.0f) stimStats.cpuUsage = 0.0f;

    // Обновляем для следующего расчета
    cpuStats.lastUpdateTime = currentTime;
    cpuStats.lastUiActiveTime = uiStats.totalActiveTimeUs;
    cpuStats.lastStimActiveTime = stimStats.totalActiveTimeUs;

    // Загрузка по ядрам
    cpuStats.core0Usage = uiStats.cpuUsage;
    cpuStats.core1Usage = stimStats.cpuUsage;
}

static void printCoreInfo() {
    Serial.println("\n╔════════════════════════════════════════════╗");
    Serial.println("║          Core Assignment Info              ║");
    Serial.println("╠════════════════════════════════════════════╣");

    Serial.printf("║ setup() on Core: %d                        ║\n", xPortGetCoreID());

    if (uiTaskHandle != nullptr) {
        BaseType_t core = xTaskGetAffinity(uiTaskHandle);
        Serial.printf("║ UI_Task on Core: %d                        ║\n",
                      (core == tskNO_AFFINITY) ? -1 : core);
    }

    if (stimTaskHandle != nullptr) {
        BaseType_t core = xTaskGetAffinity(stimTaskHandle);
        Serial.printf("║ Stim_Task on Core: %d                      ║\n",
                      (core == tskNO_AFFINITY) ? -1 : core);
    }

    Serial.println("╚════════════════════════════════════════════╝\n");
}

static float wrapDegrees(float degrees) {
    while (degrees < 0)       degrees += 360.0f;
    while (degrees >= 360.0f) degrees -= 360.0f;
    return degrees;
}

static void printTaskInfo(const char* taskName, const TaskStats& stats) {
    Serial.printf("[%s] Core:%d Loops:%lu Cmds:%lu MaxLoop:%lu µs CPU:%.1f%%\n",
                  taskName,
                  stats.coreId,
                  stats.loopCount,
                  (taskName[0] == 'U') ? stats.commandsSent : stats.commandsReceived,
                  stats.maxLoopTime,
                  stats.cpuUsage);
}

static void printSystemStats() {
    // Обновляем статистику загрузки CPU
    updateCpuUsage();

    Serial.println("\n╔════════════════════════════════════════════╗");
    Serial.println("║          System Statistics                 ║");
    Serial.println("╠════════════════════════════════════════════╣");

    // Информация о задачах
    printTaskInfo("UI_Task  ", uiStats);
    printTaskInfo("Stim_Task", stimStats);

    // Информация о стеке
    if (uiTaskHandle != nullptr) {
        UBaseType_t waterMark = uxTaskGetStackHighWaterMark(uiTaskHandle);
        Serial.printf("║ UI Stack Free: %u bytes                ║\n", waterMark * 4);
        if (waterMark < 512) {
            Serial.println("║ ⚠️  WARNING: UI Stack Low!              ║");
        }
    }

    if (stimTaskHandle != nullptr) {
        UBaseType_t waterMark = uxTaskGetStackHighWaterMark(stimTaskHandle);
        Serial.printf("║ Stim Stack Free: %u bytes              ║\n", waterMark * 4);
        if (waterMark < 512) {
            Serial.println("║ ⚠️  WARNING: Stim Stack Low!            ║");
        }
    }

    // Информация о памяти
    Serial.printf("║ Free Heap: %u bytes                    ║\n", ESP.getFreeHeap());
    Serial.printf("║ Min Free Heap: %u bytes                ║\n", ESP.getMinFreeHeap());

    // Информация о CPU
    Serial.printf("║ CPU Freq: %u MHz                       ║\n", ESP.getCpuFreqMHz());

    // Загрузка ядер CPU
    Serial.println("║                                            ║");
    Serial.printf("║ Core 0 Usage: %.1f%%                      ║\n", cpuStats.core0Usage);
    Serial.printf("║ Core 1 Usage: %.1f%%                      ║\n", cpuStats.core1Usage);

    // Проверка dual-core
    Serial.printf("║ Dual-Core: %s                          ║\n",
                  (uiStats.coreId != stimStats.coreId) ? "✅ YES" : "❌ NO");

    Serial.println("╚════════════════════════════════════════════╝\n");

    // Сброс статистики
    uiStats.maxLoopTime = 0;
    stimStats.maxLoopTime = 0;
}

 // Функция для вывода детальной информации о задачах
static void printDetailedTaskStats() {
    Serial.println("\n╔════════════════════════════════════════════════════════════════╗");
    Serial.println("║                    Detailed Task Statistics                    ║");
    Serial.println("╠════════════════════════════════════════════════════════════════╣");

    // Информация о наших задачах
    Serial.println("║ Our Tasks:                                                     ║");
    Serial.printf("║   UI_Task:   Core %d, Loops: %lu, CPU: %.1f%%\n",
                  uiStats.coreId, uiStats.loopCount, uiStats.cpuUsage);
    Serial.printf("║   Stim_Task: Core %d, Loops: %lu, CPU: %.1f%%\n",
                  stimStats.coreId, stimStats.loopCount, stimStats.cpuUsage);

    Serial.println("║                                                                ║");

    // Информация о стеке
    if (uiTaskHandle != nullptr) {
        UBaseType_t waterMark = uxTaskGetStackHighWaterMark(uiTaskHandle);
        Serial.printf("║   UI Stack Free: %u bytes\n", waterMark * 4);
    }

    if (stimTaskHandle != nullptr) {
        UBaseType_t waterMark = uxTaskGetStackHighWaterMark(stimTaskHandle);
        Serial.printf("║   Stim Stack Free: %u bytes\n", waterMark * 4);
    }

    Serial.println("║                                                                ║");

    // Общая информация о системе
    Serial.printf("║ Total Tasks: %u\n", uxTaskGetNumberOfTasks());
    Serial.printf("║ Free Heap: %u bytes\n", ESP.getFreeHeap());
    Serial.printf("║ Min Free Heap: %u bytes\n", ESP.getMinFreeHeap());
    Serial.printf("║ CPU Freq: %u MHz\n", ESP.getCpuFreqMHz());

    Serial.println("║                                                                ║");
    Serial.println("║ Note: CPU usage is approximate, based on loop counters        ║");
    Serial.println("╚════════════════════════════════════════════════════════════════╝\n");
}

// ============================================
// CORE 0: UI Task
// ============================================
void uiTask(void* parameter) {
    uiStats.coreId = xPortGetCoreID();
    uint32_t lastStatsTime = millis();

    Serial.printf("[UI_Task] Started on Core %d\n", uiStats.coreId);
    Serial.printf("[UI_Task] Stack size: %u bytes\n", UI_TASK_STACK_SIZE);

    // ✅ ЭНКОДЕР A - ИСПРАВЛЕНО: НЕ вызываем getEncoderAState() внутри lambda
    encoderA.onStep([](int8_t delta) {
        if (appState.adjustEncoderA(delta)) {
            Command cmd(CommandType::UPDATE_STIM_1_PARAMS, appState.getStimParams());

            if (commandQueue.send(cmd, 10)) {
                uiStats.commandsSent++;
                
                // ✅ КРИТИЧНО: Простой вывод без дополнительных вызовов AppState
                Serial.printf("[UI] Enc A: Δ=%d\n", delta);
            } else {
                Serial.println("[UI] WARN: Queue full!");
            }
        }
    });

    // ✅ ЭНКОДЕР B - ИСПРАВЛЕНО: НЕ вызываем getEncoderBState() внутри lambda
    encoderB.onStep([](int8_t delta) {
        if (appState.adjustEncoderB(delta)) {
            Command cmd(CommandType::UPDATE_STIM_1_PARAMS, appState.getStimParams());

             if (commandQueue.send(cmd, 10)) {
                 uiStats.commandsSent++;
                
                // ✅ КРИТИЧНО: Простой вывод без дополнительных вызовов AppState
                Serial.printf("[UI] Enc B: Δ=%d\n", delta);
            } else {
                 Serial.println("[UI] WARN: Queue full!");
            }
        }
    });

    // ✅ ИНИЦИАЛИЗАЦИЯ ЭНКОДЕРОВ
    if (!encoderA.begin()) {
        Serial.println("[UI] ERROR: Encoder A init failed!");
        vTaskDelete(nullptr);
        return;
    }

    if (!encoderB.begin()) {
        Serial.println("[UI] ERROR: Encoder B init failed!");
        vTaskDelete(nullptr);
        return;
    }

    Serial.println("[UI] ✓ Encoders initialized");
    
    // ✅ Вывод начальных значений ВНЕ lambda
    Serial.printf("[UI] Encoder A initial value: %d\n", appState.getEncoderAValue());
    Serial.printf("[UI] Encoder B initial value: %d\n", appState.getEncoderBValue());

    // Основной цикл
    while (true) {
        uint32_t loopStart = micros();
        uiStats.loopCount++;

        // ✅ Обновление обоих энкодеров
        encoderA.update();
        encoderB.update();

        // Периодическая статистика
        uint32_t now = millis();
        // if (now - lastStatsTime >= STATS_INTERVAL_MS) {
        //     lastStatsTime = now;
        //     printSystemStats();
        //     appState.printCurrentState();
        // }

        // Измерение времени
        uint32_t loopTime = micros() - loopStart;
        if (loopTime > uiStats.maxLoopTime) {
            uiStats.maxLoopTime = loopTime;
        }

        uiStats.totalActiveTimeUs += loopTime;

        vTaskDelay(pdMS_TO_TICKS(UI_TASK_DELAY_MS));
        esp_task_wdt_reset();
    }
}

// ============================================
// CORE 1: Stimulation Task
// ============================================

void stimTask(void* parameter) {
    stimStats.coreId = xPortGetCoreID();
    
    Serial.printf("[Stim_Task] Started on Core %d\n", stimStats.coreId);
    Serial.printf("[Stim_Task] Stack size: %u bytes\n", STIM_TASK_STACK_SIZE);

    if (!pwm_stim_1.begin()) {
        Serial.println("[Stim1] ERROR: Init failed!");
        vTaskDelete(nullptr);
        return;
    }

    if (!pwm_stim_2.begin()) {
        Serial.println("[Stim2] ERROR: Init failed!");
        vTaskDelete(nullptr);
        return;
    }

    Serial.println("[Stim] Initialized");
    
    // ✅ АВТОЗАПУСК ПРЯМО ЗДЕСЬ
    delay(100);  // Небольшая задержка
    
    Serial.println("[Stim1] Auto-starting pwm_stim_1...");
    pwm_stim_1.start();    
    Serial.println("[Stim1] ✅ STARTED");

    Serial.println("[Stim2] Auto-starting pwm_stim_2...");
    pwm_stim_2.start();    
    Serial.println("[Stim2] ✅ STARTED");

    appState.setStimRunning(true);

    while (true) {
        uint32_t loopStart = micros();
        
        stimStats.loopCount++;

        // Обработка команд
        Command cmd;
        while (commandQueue.receive(cmd, 0)) {
            stimStats.commandsReceived++;
            
            switch (cmd.type) {
                case CommandType::UPDATE_STIM_1_PARAMS:
                    pwm_stim_2.setParams(cmd.params.stimDuty);                                 
                    Serial.println("[Stim] Parameters updated");
                    break;
                
                case CommandType::START_STIM:
                    pwm_stim_1.start();
                    pwm_stim_2.start();
                    appState.setStimRunning(true);
                    Serial.println("[Stim] ✅ STARTED");
                    break;
                
                case CommandType::STOP_STIM:
                    pwm_stim_1.stop();
                    pwm_stim_2.stop();
                    appState.setStimRunning(false);
                    Serial.println("[Stim] ⛔ STOPPED");
                    break;
                
                case CommandType::EMERGENCY_STOP:
                    pwm_stim_1.stop();
                    pwm_stim_2.stop();
                    appState.setStimRunning(false);
                    Serial.println("[Stim] 🚨 EMERGENCY STOP!");
                    break;
                
                default:
                    break;
            }
        }

        // Обновление генератора
        if (appState.isStimRunning()) {
            pwm_stim_1.update();
            pwm_stim_2.update();
        }

        uint32_t loopTime = micros() - loopStart;
        if (loopTime > stimStats.maxLoopTime) {
            stimStats.maxLoopTime = loopTime;
        }

        if (STIM_TASK_DELAY_MS > 0) {
            vTaskDelay(pdMS_TO_TICKS(STIM_TASK_DELAY_MS));
        } else {
            taskYIELD();
        }
    }
}

// ============================================
// Setup
// ============================================
void setup() {
    Serial.begin(115200);
    Serial.setTxBufferSize(1024); // Увеличиваем буфер TX
    Serial.setRxBufferSize(256);  // Увеличиваем буфер RX

    // Ждем подключения Serial (опционально)
    uint32_t startTime = millis();
    while (!Serial && (millis() - startTime < 3000)) {
        delay(10);
    }

    delay(100);

    Serial.println("\n");
    Serial.println("╔════════════════════════════════════════════╗");
    Serial.println("║   EMS Controller Dual-Core System v1.0     ║");
    Serial.println("╚════════════════════════════════════════════╝");
    Serial.printf("Setup running on Core %d\n", xPortGetCoreID());
    Serial.printf("CPU Frequency: %u MHz\n", ESP.getCpuFreqMHz());
    Serial.printf("Free Heap: %u bytes\n", ESP.getFreeHeap());

    // GPIO
    pinMode(PWM_STATE_PIN, OUTPUT);
    digitalWrite(PWM_STATE_PIN, LOW);
    Serial.println("✓ GPIO initialized");

    // Command Queue
    if (!commandQueue.isValid()) {
        Serial.println("✗ ERROR: Failed to create command queue!");
        return;
    }
    Serial.println("✓ Command queue created");

    // Watchdog
    esp_task_wdt_init(WDT_TIMEOUT_SEC, true);
    Serial.printf("✓ Watchdog configured (%d sec timeout)\n", WDT_TIMEOUT_SEC);

    // UI Task на Core 0
    BaseType_t result = xTaskCreatePinnedToCore(
        uiTask,
        "UI_Task",
        UI_TASK_STACK_SIZE,
        nullptr,
        1,
        &uiTaskHandle,
        0
    );

    if (result != pdPASS) {
        Serial.println("✗ ERROR: Failed to create UI task!");
        return;
    }
    Serial.printf("✓ UI Task created on Core 0 (Stack: %u bytes, Priority: 1)\n",
                  UI_TASK_STACK_SIZE);

    // Stim Task на Core 1
    result = xTaskCreatePinnedToCore(
        stimTask,
        "Stim_Task",
        STIM_TASK_STACK_SIZE,
        nullptr,
        2,
        &stimTaskHandle,
        1
    );

    if (result != pdPASS) {
        Serial.println("✗ ERROR: Failed to create Stim task!");
        return;
    }
    Serial.printf("✓ Stim Task created on Core 1 (Stack: %u bytes, Priority: 2)\n",
                  STIM_TASK_STACK_SIZE);

    // Задержка для инициализации задач
    delay(200);

    // Проверка привязки к ядрам
    printCoreInfo();

    Serial.println("\n╔════════════════════════════════════════════╗");
    Serial.println("║          System Ready!                     ║");
    Serial.println("╚════════════════════════════════════════════╝\n");

    appState.printCurrentState();
    
    // ✅ АВТОМАТИЧЕСКИЙ ЗАПУСК
    Serial.println("\n[Setup] Sending initial parameters...");
    
    // Отправляем начальные параметры
    // StimParams params = appState.getStimParams();
    // Command updateCmd(CommandType::UPDATE_PARAMS, params);
    // if (commandQueue.send(updateCmd, 100)) {
    //     Serial.println("[Setup] ✓ Initial parameters sent");
    // } else {
    //     Serial.println("[Setup] ✗ Failed to send parameters!");
    // }
    
    // Задержка для обработки
    delay(50);
    
    // Автоматически запускаем стимуляцию
    Command startCmd(CommandType::START_STIM);
    if (commandQueue.send(startCmd, 100)) {
        Serial.println("[Setup] ✓ Stimulation AUTO-STARTED");
    } else {
        Serial.println("[Setup] ✗ Failed to send START!");
    }
    
    Serial.println("\n🎛️  Rotate encoder to adjust parameters");
    Serial.println("📊 Statistics every 10 seconds\n");
}

// ============================================
// Loop
// ============================================
void loop() {
    // loop() работает на Core 1, но мы его не используем
    // Спим, чтобы не нагружать CPU
    vTaskDelay(pdMS_TO_TICKS(1000));
}