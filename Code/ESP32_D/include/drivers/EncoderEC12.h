#pragma once
#include <Arduino.h>

#include  ".\core\IEncoder.h"

// Реализация инкрементного энкодера EC12 (2 канала: CLK/DT)
// Алгоритм: определение направления на фронтах/спадах CLK по уровню DT.
class EncoderEC12 : public IEncoder {
public:
    // Порядок аргументов: clkPin, dtPin
    // debounceUs — антидребезг в микросекундах (например, 1000 = 1мс)
    EncoderEC12(uint8_t clkPin, uint8_t dtPin, uint32_t debounceUs = 1000);

    bool begin() override;
    void update() override;
    void onStep(StepHandler h) override { handler_ = std::move(h); }

private:
    static void IRAM_ATTR isrThunk(void* arg);
    void IRAM_ATTR handleIsr();

private:
    uint8_t clkPin_;
    uint8_t dtPin_;
    uint32_t debounceUs_{1000};

    volatile int lastClk_{HIGH};
    volatile uint32_t lastDebounceUs_{0};

    // Накопленные шаги из ISR
    volatile int32_t pendingSteps_{0};

    // Критическая секция для обмена с ISR
    portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;

    StepHandler handler_{};
};