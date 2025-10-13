#pragma once
#include <Arduino.h>

#include "core/IEncoder.h"

// Реализация энкодера C14 с кастомной логикой обработки
class EncoderC14 : public IEncoder {
public:
    // Используем конструктор базового класса
    using IEncoder::IEncoder;

    // Все общие методы (begin, update, onStep) наследуются из IEncoder
    // Переопределяем только специфичную логику обработки прерывания

protected:
    // Переопределяем виртуальный метод для специфичной логики C14
    void IRAM_ATTR handleIsr() override;
};