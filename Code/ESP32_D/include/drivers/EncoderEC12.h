#pragma once
#include <Arduino.h>

#include "core/IEncoder.h"

// Реализация инкрементного энкодера EC12 (2 канала: CLK/DT)
// Алгоритм: определение направления на фронтах/спадах CLK по уровню DT.
class EncoderEC12 : public IEncoder {
public:
    // Используем конструктор базового класса
    using IEncoder::IEncoder;

    // Все общие методы (begin, update, onStep) наследуются из IEncoder
    // Переопределяем только специфичную логику обработки прерывания

protected:
    // Переопределяем виртуальный метод для специфичной логики EC12
    void IRAM_ATTR handleIsr() override;
        
    void setupInterrupts() override;
};