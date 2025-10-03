#pragma once
#include <stdint.h>

#include <functional>

class IEncoder {
public:
    using StepHandler = std::function<void(int8_t delta)>; // вызывается НЕ из ISR

    virtual ~IEncoder() = default;

    // Инициализация (настройка пинов/ISR)
    virtual bool begin() = 0;

    // Вызывается из loop()/задачи — вычитывает накопленные шаги и вызывает handler
    virtual void update() = 0;

    // Регистрация обработчика шагов; вызывается в контексте потока, не ISR
    virtual void onStep(StepHandler h) = 0;
};