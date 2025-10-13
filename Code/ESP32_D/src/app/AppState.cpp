#include "app/AppState.h"

AppState::AppState() 
    : stimRunning_(false)
{
    mutex_ = xSemaphoreCreateMutex();
    if (mutex_ == nullptr) {
        Serial.println("[AppState] ERROR: Failed to create mutex!");
    }
    
    // Инициализация начальных значений
    encoderAState_.value = 10;
    encoderAState_.position = 0;

    encoderBState_.value = 10;
    encoderBState_.position = 0;

    stimParams_.stimDuty = 10;
}

AppState::~AppState() {
    if (mutex_ != nullptr) {
        vSemaphoreDelete(mutex_);
    }
}

StimParams AppState::getStimParams() const {
    StimParams params;
    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(100)) == pdTRUE) {
        params = stimParams_;
        xSemaphoreGive(mutex_);
    }
    return params;
}

void AppState::setStimParams(const StimParams& params) {
    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(100)) == pdTRUE) {
        stimParams_ = params;
        applyConstraints(stimParams_);
        xSemaphoreGive(mutex_);
    }
}

uint8_t AppState::getAmplitude() const {
    uint8_t amp = 0;
    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(100)) == pdTRUE) {
        amp = stimParams_.stimDuty;
        xSemaphoreGive(mutex_);
    }
    return amp;
}

void AppState::setAmplitude(uint8_t amp) {
    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(100)) == pdTRUE) {
        stimParams_.stimDuty = constrain(amp, 0, 100);
        xSemaphoreGive(mutex_);
    }
}

// === Методы для Энкодера A ===

bool AppState::adjustEncoderA(int8_t delta) {
    bool changed = false;
    
    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(100)) == pdTRUE) {
        encoderAState_.position += delta;
        
        // Изменяем значение с ограничением 0-100
        int newValue = (int)encoderAState_.value + delta;
        newValue = constrain(newValue, 0, 100);
        
        if (newValue != encoderAState_.value) {
            encoderAState_.value = newValue;
            // Обновляем общий amplitude
            stimParams_.stimDuty = encoderAState_.value;
            changed = true;
        }
        
        xSemaphoreGive(mutex_);
    } else {
        // Таймаут - не логируем каждый раз, чтобы не забивать Serial
        static uint32_t lastWarnTime = 0;
        uint32_t now = millis();
        if (now - lastWarnTime > 1000) {
            lastWarnTime = now;
            Serial.println("[AppState] WARN: Mutex timeout in adjustEncoderA");
        }
    }
    
    return changed;
}

EncoderState AppState::getEncoderAState() const {
    EncoderState state;
    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(100)) == pdTRUE) {
        state = encoderAState_;
        xSemaphoreGive(mutex_);
    }
    return state;
}

uint8_t AppState::getEncoderAValue() const {
    uint8_t value = 0;
    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(100)) == pdTRUE) {
        value = encoderAState_.value;
        xSemaphoreGive(mutex_);
    }
    return value;
}

// === Методы для Энкодера B ===

bool AppState::adjustEncoderB(int8_t delta) {
    bool changed = false;
    
    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(100)) == pdTRUE) {
        encoderBState_.position += delta;
        
        // Изменяем значение с ограничением 0-100
        int newValue = (int)encoderBState_.value + delta;
        newValue = constrain(newValue, 0, 100);
        
        if (newValue != encoderBState_.value) {
            encoderBState_.value = newValue;
            // Обновляем общий amplitude
            //stimParams_.amplitude = encoderBState_.value;
            changed = true;
        }
        
        xSemaphoreGive(mutex_);
    } else {
        // Таймаут - не логируем каждый раз
        static uint32_t lastWarnTime = 0;
        uint32_t now = millis();
        if (now - lastWarnTime > 1000) {
            lastWarnTime = now;
            Serial.println("[AppState] WARN: Mutex timeout in adjustEncoderB");
        }
    }
    
    return changed;
}

EncoderState AppState::getEncoderBState() const {
    EncoderState state;
    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(100)) == pdTRUE) {
        state = encoderBState_;
        xSemaphoreGive(mutex_);
    }
    return state;
}

uint8_t AppState::getEncoderBValue() const {
    uint8_t value = 0;
    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(100)) == pdTRUE) {
        value = encoderBState_.value;
        xSemaphoreGive(mutex_);
    }
    return value;
}

// === Вспомогательные методы ===

void AppState::printCurrentState() const {
    // ✅ КРИТИЧНО: Захватываем мьютекс ОДИН РАЗ для всех данных
    StimParams params;
    EncoderState encA;
    EncoderState encB;
    bool running = isStimRunning();
    
    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(100)) == pdTRUE) {
        params = stimParams_;
        encA = encoderAState_;
        encB = encoderBState_;
        xSemaphoreGive(mutex_);
        
        // Вывод ПОСЛЕ освобождения мьютекса
        Serial.println("╔════════════════════════════════════════════════════╗");
        Serial.printf( "║ Encoder A: value=%3d  position=%6ld          ║\n", 
                       encA.value, encA.position);
        Serial.printf( "║ Encoder B: value=%3d  position=%6ld          ║\n", 
                       encB.value, encB.position);
        Serial.println("╠════════════════════════════════════════════════════╣");
        Serial.printf( "║ Final Amplitude: %3d%%                            ║\n", 
                       params.stimDuty);
        Serial.printf( "║ Stim Running: %s                                 ║\n", 
                       running ? "YES" : "NO ");
        Serial.println("╚════════════════════════════════════════════════════╝");
    }
}

void AppState::applyConstraints(StimParams& params) const {
    params.stimDuty = constrain(params.stimDuty, 0, 100);
}