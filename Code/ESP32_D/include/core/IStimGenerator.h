#pragma once
#include <stdint.h>

#include <functional>

class IStimGenerator {
public:

    virtual ~IStimGenerator() = default;
    virtual bool begin() = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void setParams(uint8_t amplitudePercent) = 0;
    virtual void update() = 0;
};