#pragma once
#include <Arduino.h>

#include  ".\core\IStimGenerator.h"

// Реализация инкрементного энкодера EC12 (2 канала: CLK/DT)
// Алгоритм: определение направления на фронтах/спадах CLK по уровню DT.
class EMSPulseGenerator : public IStimGenerator {
public:

    EMSPulseGenerator();
    
    bool begin() override;
    void start() override;
    void stop() override;

    void setParams(uint8_t amplitudePercent,
                 uint16_t pulseWidthUs,
                 uint8_t rateHz,
                 float burstHz,
                 uint8_t burstDutyPercent) override;

    void update() override;

private:
    uint8_t  amp_ = 0;           // %
    uint16_t pwUs_ = 200;        // микросек
    uint8_t  rateHz_ = 20;       // Гц
    float    bHz_ = 1.0f;        // Гц
    uint8_t  bDuty_ = 20;        // %

    // Производные
    uint32_t pulsePeriodUs_ = 50000; // 20 Гц
    uint32_t burstPeriodUs_ = 1000000; // 1 Гц
    uint32_t burstOnUs_     = 200000;  // 20%
    uint16_t ampDuty_       = 0;       // 0..1023

    // Состояние
    bool     running_ = false;
    bool     pulseActive_ = false;
    uint32_t lastPulseTs_ = 0;
    uint32_t lastBurstTs_ = 0;

};

