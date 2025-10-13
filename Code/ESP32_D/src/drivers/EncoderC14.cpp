#include "drivers/EncoderC14.h"

#include "app/pins.h"

void EncoderC14::setupInterrupts() {
    // C14 требует прерывания на обоих каналах
    attachInterruptArg(digitalPinToInterrupt(clkPin_), &IEncoder::isrThunk, this, CHANGE);
    attachInterruptArg(digitalPinToInterrupt(dtPin_), &IEncoder::isrThunk, this, CHANGE);
}

void IRAM_ATTR EncoderC14::handleIsr() {
    //const uint32_t now = micros();
    // if (now - lastDebounceUs_ < debounceUs_) {
    //     //digitalWrite(PWM_STATE_PIN, !digitalRead(PWM_STATE_PIN));  // Toggle
    //     digitalWrite(PWM_STATE_PIN, LOW);  // Toggle
    //     return;
    // }
    // digitalWrite(PWM_STATE_PIN, HIGH);  // Toggle
    //lastDebounceUs_ = now;

    const int currentCLK = digitalRead(clkPin_);
    const int currentDT  = digitalRead(dtPin_);

    // ========================================
    // ЛОГИКА C14 - ТАБЛИЦА ПЕРЕХОДОВ
    // ========================================
    
    // Формируем текущее и предыдущее состояние как 2-битные числа
    // Биты: [CLK, DT]
    int prevState = (lastClk_ << 1) | lastDt_;
    int currState = (currentCLK << 1) | currentDT;
    
    // Таблица переходов для квадратурного энкодера
    // Индекс: (prevState << 2) | currState
    // Значение: направление вращения (-1, 0, +1)
    static const int8_t transitionTable[16] = {
        0,  -1,  1,  0,   // 00 -> 00, 01, 10, 11
        1,   0,  0, -1,   // 01 -> 00, 01, 10, 11
       -1,   0,  0,  1,   // 10 -> 00, 01, 10, 11
        0,   1, -1,  0    // 11 -> 00, 01, 10, 11
    };
    
    int transition = (prevState << 2) | currState;
    int8_t step = transitionTable[transition];
    
    // Обновляем последнее состояние
    lastClk_ = currentCLK;
    lastDt_ = currentDT;
    
    // Обновляем счетчик шагов
    if (step != 0) {
        portENTER_CRITICAL_ISR(&mux_);
        digitalWrite(PWM_STATE_PIN, !digitalRead(PWM_STATE_PIN));  // Toggle
        pendingSteps_ += step;
        portEXIT_CRITICAL_ISR(&mux_);
    }
}