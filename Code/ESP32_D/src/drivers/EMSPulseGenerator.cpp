#include <Arduino.h>

#include ".\app\pins.h"
#include "..\include\drivers\EMSPulseGenerator.h"

// ====== Настройки вывода для стимуляции ======
static const int PWM1_CH      = 0;     // ledc канал
static const int PWM1_RES     = 10;    // 10 бит (0..1023)
static const int PWM1_FREQ    = 144;  // несущая для ШИМ амплитуды

EMSPulseGenerator::EMSPulseGenerator() {
    // Инициализация с вашими параметрами
    amp_ = 10;
    pwUs_ = 200;
    rateHz_ = 144;
    pulsesPerBurst_ = 26;
    pauseBetweenBurstsMs_ = 235;
    
    // Расчет производных параметров
    pulsePeriodUs_ = 1000000UL / rateHz_;  // 6944 мкс
    burstDurationUs_ = pulsesPerBurst_ * pulsePeriodUs_;  // 180556 мкс
    pauseDurationUs_ = pauseBetweenBurstsMs_ * 1000UL;    // 235000 мкс
    fullCycleUs_ = burstDurationUs_ + pauseDurationUs_;   // 415556 мкс
    
    ampDuty_ = map(amp_, 0, 100, 0, 1023);
}

bool EMSPulseGenerator::begin() {
    // Настройка LEDC для ШИМ
    ledcSetup(PWM1_CH, PWM1_FREQ, PWM1_RES);
    ledcAttachPin(PWM_CH_1_PIN, PWM1_CH);
    ledcWrite(PWM1_CH, 0);
    
    // Инициализация временных меток
    lastPulseTs_ = micros();
    burstStartTs_ = lastPulseTs_;
    cycleStartTs_ = lastPulseTs_;
    running_ = false;
    
    Serial.println("[EMS] Initialized with parameters:");
    Serial.printf("  Pulse rate: %d Hz (period: %lu µs)\n", rateHz_, pulsePeriodUs_);
    Serial.printf("  Pulses per burst: %d\n", pulsesPerBurst_);
    Serial.printf("  Burst duration: %lu µs (%.1f ms)\n", burstDurationUs_, burstDurationUs_ / 1000.0f);
    Serial.printf("  Pause duration: %lu µs (%lu ms)\n", pauseDurationUs_, pauseBetweenBurstsMs_);
    Serial.printf("  Full cycle: %lu µs (%.1f ms, %.2f Hz)\n", 
                  fullCycleUs_, fullCycleUs_ / 1000.0f, 1000000.0f / fullCycleUs_);
    
    return true;
}

void EMSPulseGenerator::start() {
    running_ = true;
    // Сброс таймеров для корректного старта
    uint32_t now = micros();
    lastPulseTs_ = now;
    burstStartTs_ = now;
    cycleStartTs_ = now;
    nextPulseTs_ = now;  // ✅ НОВОЕ: расчетное время следующего импульса
    pulseActive_ = false;
    pulseCountInBurst_ = 0;
    inBurst_ = true;
    
    Serial.println("[EMS] ✅ Started");
}

void EMSPulseGenerator::stop() {
    running_ = false;
    pulseActive_ = false;
    inBurst_ = false;
    ledcWrite(PWM1_CH, 0);
    digitalWrite(PWM_STATE_PIN, LOW);
    
    Serial.println("[EMS] ⛔ Stopped");
}

void EMSPulseGenerator::setParams(uint8_t amplitudePercent,
                                  uint16_t pulseWidthUs,
                                  uint8_t rateHz,
                                  float burstHz,
                                  uint8_t burstDutyPercent) {
    // Применяем только амплитуду и длительность импульса
    amp_   = constrain(amplitudePercent, 0, 100);
    pwUs_  = constrain(pulseWidthUs, 50, 500);
    
    // Преобразование амплитуды в значение ШИМ (0..1023)
    // ampDuty_ = map(amp_, 0, 100, 0, (1 << PWM1_RES) - 1);
    // if (amp_ <= 0) ampDuty_ = 0;

    ampDuty_ = 8;

    Serial.printf("income = %d , ampDuty = %d ", amplitudePercent, ampDuty_);
    // ❌ УБРАЛИ Serial.printf отсюда - он вызывается слишком часто!
}

void EMSPulseGenerator::update() {
    if (!running_) return;

    const uint32_t now = micros();
    
    // Определяем положение в полном цикле
    const uint32_t cycleElapsed = now - cycleStartTs_;
    
    // Проверка завершения полного цикла
    //fullCycleUs_ = 415556;     // 180556 + 235000 = 415556 мкс
    if (cycleElapsed >= fullCycleUs_) {
        // Начинаем новый цикл
        cycleStartTs_ = now;
        burstStartTs_ = now;
        nextPulseTs_ = now;
        pulseCountInBurst_ = 0;
        inBurst_ = true;
        pulseActive_ = false;
        ledcWrite(PWM1_CH, 0);
       // digitalWrite(PWM_STATE_PIN, LOW);
        
       // Serial.printf("[EMS] 🔄 New cycle at %lu µs\n", now);
        return;
    }
    
    // Определяем, находимся ли мы в пачке или в паузе
    const uint32_t burstElapsed = now - burstStartTs_;
    
    //burstDurationUs_ = 180556; // 26 × 6944 = 180556 мкс
    if (burstElapsed >= burstDurationUs_) {
        // Мы в паузе между пачками
        if (inBurst_) {
            inBurst_ = false;
            pulseActive_ = false;
            ledcWrite(PWM1_CH, 0);
            digitalWrite(PWM_STATE_PIN, LOW);
           // Serial.printf("[EMS] 💤 Pause (sent %d pulses)\n", pulseCountInBurst_);
        }
        else{
          
        }
        return;
    }
    
    // Мы внутри пачки - проверяем количество импульсов
    // if (pulseCountInBurst_ >= pulsesPerBurst_) {
    //     // Достигли лимита импульсов в пачке
    //     if (inBurst_) {
    //         inBurst_ = false;
    //         pulseActive_ = false;
    //         ledcWrite(PWM1_CH, 0);
    //         //digitalWrite(PWM_STATE_PIN, LOW);
    //         //Serial.printf("[EMS] ✅ Burst complete (%d pulses)\n", pulseCountInBurst_);
    //     }
    //     else{
    //       //digitalWrite(PWM_STATE_PIN, HIGH);
          
    //     }
    //     return;
    // }
    
    // ✅ ИСПРАВЛЕННАЯ ЛОГИКА: Проверяем состояние импульса
    if (!pulseActive_) {
        // ✅ Ждем времени начала следующего импульса
        //if (now >= nextPulseTs_) {
            // ✅ НАЧИНАЕМ НОВЫЙ ИМПУЛЬС - ОДИН РАЗ!
            pulseActive_ = true;
            pulseCountInBurst_++;
            lastPulseTs_ = now;
            nextPulseTs_ += pulsePeriodUs_;  // Планируем следующий импульс
            
            // ✅ Включаем PWM - ОДИН РАЗ!
            ledcWrite(PWM1_CH, ampDuty_);
            digitalWrite(PWM_STATE_PIN, HIGH);

            //digitalWrite(PWM_STATE_PIN, !digitalRead(PWM_STATE_PIN));  // Toggle
            
            // Отладка первых 3 импульсов
            // if (pulseCountInBurst_ <= 3) {
            //     Serial.printf("[EMS] Pulse #%d START at %lu µs (next: %lu µs)\n", 
            //                   pulseCountInBurst_, now, nextPulseTs_);
            // }
        //}
        // ✅ Если now < nextPulseTs_, просто ждем - ничего не делаем
    } else {
        // ✅ ИМПУЛЬС УЖЕ АКТИВЕН - проверяем не пора ли его выключить
        const uint32_t sincePulse = now - lastPulseTs_;
        //if (sincePulse >= pwUs_) {
            // ✅ ВЫКЛЮЧАЕМ ИМПУЛЬС - ОДИН РАЗ!
        //    pulseActive_ = false;
        //    ledcWrite(PWM1_CH, 0);
            // digitalWrite для toggle не трогаем
            
            // Отладка первых 3 импульсов
            // if (pulseCountInBurst_ <= 3) {
            //     Serial.printf("[EMS] Pulse #%d END at %lu µs (duration: %lu µs)\n", 
            //                   pulseCountInBurst_, now, sincePulse);
           // }
       // }
        // ✅ Если sincePulse < pwUs_, импульс еще активен - ничего не делаем
    }
}