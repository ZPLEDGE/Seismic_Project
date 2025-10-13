#include "core/IEncoder.h"
#include "app/pins.h"

bool IEncoder::begin() {
    pinMode(clkPin_, INPUT_PULLUP);
    pinMode(dtPin_,  INPUT_PULLUP);

    lastClk_ = digitalRead(clkPin_);
    lastDt_ = digitalRead(dtPin_);
    lastDebounceUs_ = micros();

    // Вызываем виртуальный метод для настройки прерываний
    setupInterrupts();
    
    return true;
}

void IRAM_ATTR IEncoder::isrThunk(void* arg) {
    static_cast<IEncoder*>(arg)->handleIsr();
}

void IEncoder::update() {
    int32_t delta = 0;
    taskENTER_CRITICAL(&mux_);
    delta = pendingSteps_;
    pendingSteps_ = 0;
    taskEXIT_CRITICAL(&mux_);

    if (delta != 0 && handler_) {
        // Склеенный вызов: один колбэк с суммарным сдвигом
        // Ограничиваем диапазон int8_t (-127..127)
        delta = std::max<int32_t>(-127, std::min<int32_t>(127, delta));
        handler_(static_cast<int8_t>(delta));
    }
}