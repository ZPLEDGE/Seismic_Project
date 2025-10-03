#include "..\include\drivers\EncoderEC12.h"
#include ".\app\pins.h"

EncoderEC12::EncoderEC12(uint8_t clkPin, uint8_t dtPin, uint32_t debounceUs)
: clkPin_(clkPin), dtPin_(dtPin), debounceUs_(debounceUs) {}

bool EncoderEC12::begin() {
    pinMode(clkPin_, INPUT_PULLUP);
    pinMode(dtPin_,  INPUT_PULLUP);

    lastClk_ = digitalRead(clkPin_);
    lastDebounceUs_ = micros();

    // Отдельное прерывание на CLK с передачей this
    attachInterruptArg(digitalPinToInterrupt(clkPin_), &EncoderEC12::isrThunk, this, CHANGE);
    return true;
}

void IRAM_ATTR EncoderEC12::isrThunk(void* arg) {
    static_cast<EncoderEC12*>(arg)->handleIsr();
}

void IRAM_ATTR EncoderEC12::handleIsr() {

     // Toggle LED при каждом прерывании
//    digitalWrite(LED_PIN, !digitalRead(LED_PIN));

    const uint32_t now = micros();
    if (now - lastDebounceUs_ < debounceUs_) {
        return; // антидребезг
    }
    lastDebounceUs_ = now;

    const int currentCLK = digitalRead(clkPin_);
    const int currentDT  = digitalRead(dtPin_);

    if (currentCLK != lastClk_) {
        if (currentCLK == LOW) {
            // Logic: DT=HIGH => шаг -, иначе +
            const int step = (currentDT == HIGH) ? - 1 : 1;
            portENTER_CRITICAL_ISR(&mux_);
            pendingSteps_ += step;
            portEXIT_CRITICAL_ISR(&mux_);
        }
        lastClk_ = currentCLK;
    }
}

void EncoderEC12::update() {
    int32_t delta = 0;
    taskENTER_CRITICAL(&mux_);
    delta = pendingSteps_;
    pendingSteps_ = 0;
    taskEXIT_CRITICAL(&mux_);

    if (delta != 0 && handler_) {
        // Склеенный вызов: один колбэк с суммарным сдвигом
        delta = std::max<int32_t>(-127, std::min<int32_t>(127, delta));
        handler_(static_cast<int8_t>(delta));
    }
}