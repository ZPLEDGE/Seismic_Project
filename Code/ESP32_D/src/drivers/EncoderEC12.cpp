#include "drivers/EncoderEC12.h"

void EncoderEC12::setupInterrupts() {
    // Базовая реализация: только CLK (для EC12)
    attachInterruptArg(digitalPinToInterrupt(clkPin_), &IEncoder::isrThunk, this, CHANGE);
}

void IRAM_ATTR EncoderEC12::handleIsr() {
    // const uint32_t now = micros();
    // if (now - lastDebounceUs_ < debounceUs_) {
    //     return;
    // }
    // lastDebounceUs_ = now;

    const int currentCLK = digitalRead(clkPin_);
    const int currentDT  = digitalRead(dtPin_);

    // ========================================
    // СТАНДАРТНАЯ ЛОГИКА ДЛЯ EC12
    // Обрабатываем только изменения CLK
    // ========================================
    if (currentCLK != lastClk_) {
        if (currentCLK == LOW) {
            // Стандартная логика EC12: проверяем DT при падении CLK
            const int step = (currentDT == HIGH) ? -1 : 1;
            
            portENTER_CRITICAL_ISR(&mux_);
            pendingSteps_ += step;
            portEXIT_CRITICAL_ISR(&mux_);
        }
        lastClk_ = currentCLK;
    }
    
    // Обновляем lastDt_ для совместимости
    lastDt_ = currentDT;
}