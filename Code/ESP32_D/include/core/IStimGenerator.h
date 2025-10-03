#pragma once
#include <stdint.h>

#include <functional>

class IStimGenerator {
public:

    virtual ~IStimGenerator() = default;
    virtual bool begin() = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void setParams(uint8_t amplitudePercent,
                            uint16_t pulseWidthUs,
                            uint8_t rateHz,
                            float burstHz,
                            uint8_t burstDutyPercent) = 0;
    virtual void update() = 0;
};