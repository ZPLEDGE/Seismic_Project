#pragma once
#include <Arduino.h>

#include "core/IEncoder.h"

// Реализация энкодера C14 с кастомной логикой обработки
// Использует прерывания на обоих каналах (CLK и DT)
class EncoderC14 : public IEncoder {
public:
    // Используем конструктор базового класса
    using IEncoder::IEncoder;

    // Все общие методы (begin, update, onStep) наследуются из IEncoder
    // Переопределяем специфичную логику обработки прерывания и настройки

protected:
    // Переопределяем виртуальный метод для специфичной логики C14
    void IRAM_ATTR handleIsr() override;
    
    // Переопределяем настройку прерываний - C14 нужны оба канала
    void setupInterrupts() override;
};