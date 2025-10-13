#pragma once
#include <stdint.h>
#include <functional>
#include <Arduino.h>

class IEncoder {
public:
    using StepHandler = std::function<void(int8_t delta)>; // вызывается НЕ из ISR

    // Общий конструктор для всех энкодеров
    // clkPin - пин CLK (A)
    // dtPin - пин DT (B)
    // debounceUs - антидребезг в микросекундах (по умолчанию 1000 = 1мс)
    IEncoder(uint8_t clkPin, uint8_t dtPin, uint32_t debounceUs = 1000)
        : clkPin_(clkPin)
        , dtPin_(dtPin)
        , debounceUs_(debounceUs)
    {}

    virtual ~IEncoder() = default;

    // Инициализация (настройка пинов/ISR) - теперь реализована в базовом классе
    virtual bool begin();

    // Вызывается из loop()/задачи — вычитывает накопленные шаги и вызывает handler
    virtual void update();

    // Регистрация обработчика шагов; вызывается в контексте потока, не ISR
    virtual void onStep(StepHandler h) { handler_ = std::move(h); }

    // Геттеры для доступа к параметрам
    uint8_t getClkPin() const { return clkPin_; }
    uint8_t getDtPin() const { return dtPin_; }
    uint32_t getDebounceUs() const { return debounceUs_; }

protected:
    // Виртуальный метод для обработки прерывания - переопределяется в производных классах
    virtual void IRAM_ATTR handleIsr() = 0;

    // Статический thunk для вызова handleIsr из прерывания
    static void IRAM_ATTR isrThunk(void* arg);

protected:
    // Параметры энкодера
    uint8_t clkPin_;
    uint8_t dtPin_;
    uint32_t debounceUs_;

    // Состояние для обработки прерываний
    volatile int lastClk_{HIGH};
    volatile uint32_t lastDebounceUs_{0};

    // Накопленные шаги из ISR
    volatile int32_t pendingSteps_{0};

    // Критическая секция для обмена с ISR
    portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;

    // Обработчик шагов
    StepHandler handler_{};
};