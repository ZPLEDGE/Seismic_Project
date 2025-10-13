#include "drivers/EncoderC14.h"

void IRAM_ATTR EncoderC14::handleIsr() {
    const uint32_t now = micros();
    if (now - lastDebounceUs_ < debounceUs_) {
        return;
    }
    lastDebounceUs_ = now;

    const int currentCLK = digitalRead(clkPin_);
    const int currentDT  = digitalRead(dtPin_);

    // ========================================
    // КАСТОМНАЯ ЛОГИКА ДЛЯ C14
    // ========================================
    if (currentCLK != lastClk_) {
        if (currentCLK == LOW) {
            // Инвертированная логика по сравнению с EC12
            const int step = (currentDT == LOW) ? -1 : 1;
            
            // Или удвоенные шаги:
            // const int step = (currentDT == HIGH) ? -2 : 2;
            
            // Или детекция только одного направления:
            // const int step = (currentDT == HIGH) ? 1 : 0;
            
            portENTER_CRITICAL_ISR(&mux_);
            pendingSteps_ += step;
            portEXIT_CRITICAL_ISR(&mux_);
        }
        lastClk_ = currentCLK;
    }
}