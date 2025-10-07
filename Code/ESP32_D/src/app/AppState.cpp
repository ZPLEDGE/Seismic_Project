#include "app/AppState.h"

AppState::AppState() 
    : currentParam_(ParamType::AMPLITUDE)
    , stimRunning_(false)
{
    mutex_ = xSemaphoreCreateMutex();
    if (mutex_ == nullptr) {
        Serial.println("[AppState] ERROR: Failed to create mutex!");
    }
}

AppState::~AppState() {
    if (mutex_ != nullptr) {
        vSemaphoreDelete(mutex_);
    }
}

StimParams AppState::getStimParams() const {
    StimParams params;
    if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        params = stimParams_;
        xSemaphoreGive(mutex_);
    }
    return params;
}

void AppState::setStimParams(const StimParams& params) {
    if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        stimParams_ = params;
        applyConstraints(stimParams_);
        xSemaphoreGive(mutex_);
    }
}

bool AppState::adjustCurrentParam(int8_t delta) {
    bool changed = false;
    
    if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        switch (currentParam_) {
            case ParamType::AMPLITUDE:
                stimParams_.amplitude = constrain(
                    (int)stimParams_.amplitude + delta, 0, 100);
                changed = true;
                break;
                
            case ParamType::PULSE_WIDTH:
                stimParams_.pulseWidthUs = constrain(
                    (int)stimParams_.pulseWidthUs + delta * 10, 50, 400);
                changed = true;
                break;
                
            case ParamType::RATE:
                stimParams_.rateHz = constrain(
                    (int)stimParams_.rateHz + delta, 10, 100);
                changed = true;
                break;
                
            case ParamType::BURST_HZ:
                stimParams_.burstHz = constrain(
                    stimParams_.burstHz + delta * 0.1f, 0.5f, 5.0f);
                changed = true;
                break;
                
            case ParamType::BURST_DUTY:
                stimParams_.burstDutyPercent = constrain(
                    (int)stimParams_.burstDutyPercent + delta, 10, 50);
                changed = true;
                break;
        }
        xSemaphoreGive(mutex_);
    }
    
    return changed;
}

void AppState::selectNextParam() {
    if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        int next = (int)currentParam_ + 1;
        if (next > (int)ParamType::BURST_DUTY) {
            next = 0;
        }
        currentParam_ = (ParamType)next;
        xSemaphoreGive(mutex_);
    }
}

void AppState::selectPrevParam() {
    if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        int prev = (int)currentParam_ - 1;
        if (prev < 0) {
            prev = (int)ParamType::BURST_DUTY;
        }
        currentParam_ = (ParamType)prev;
        xSemaphoreGive(mutex_);
    }
}

void AppState::setCurrentParam(ParamType param) {
    if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        currentParam_ = param;
        xSemaphoreGive(mutex_);
    }
}

ParamType AppState::getCurrentParam() const {
    ParamType param = ParamType::AMPLITUDE;
    if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        param = currentParam_;
        xSemaphoreGive(mutex_);
    }
    return param;
}

void AppState::printCurrentState() const {
    StimParams params = getStimParams();
    ParamType current = getCurrentParam();
    
    Serial.println("╔════════════════════════════════════╗");
    Serial.printf( "║ Current Param: %-19s ║\n", getParamName(current));
    Serial.println("╠════════════════════════════════════╣");
    Serial.printf( "║ Amplitude:     %3d%%               ║\n", params.amplitude);
    Serial.printf( "║ Pulse Width:   %3d µs             ║\n", params.pulseWidthUs);
    Serial.printf( "║ Rate:          %3d Hz             ║\n", params.rateHz);
    Serial.printf( "║ Burst Hz:      %4.1f Hz            ║\n", params.burstHz);
    Serial.printf( "║ Burst Duty:    %3d%%               ║\n", params.burstDutyPercent);
    Serial.println("╚════════════════════════════════════╝");
}

const char* AppState::getParamName(ParamType type) const {
    switch(type) {
        case ParamType::AMPLITUDE:    return "AMPLITUDE";
        case ParamType::PULSE_WIDTH:  return "PULSE_WIDTH";
        case ParamType::RATE:         return "RATE";
        case ParamType::BURST_HZ:     return "BURST_HZ";
        case ParamType::BURST_DUTY:   return "BURST_DUTY";
        default:                      return "UNKNOWN";
    }
}

String AppState::getCurrentParamValueStr() const {
    StimParams params = getStimParams();
    ParamType current = getCurrentParam();
    
    switch(current) {
        case ParamType::AMPLITUDE:
            return String(params.amplitude) + "%";
        case ParamType::PULSE_WIDTH:
            return String(params.pulseWidthUs) + "µs";
        case ParamType::RATE:
            return String(params.rateHz) + "Hz";
        case ParamType::BURST_HZ:
            return String(params.burstHz, 1) + "Hz";
        case ParamType::BURST_DUTY:
            return String(params.burstDutyPercent) + "%";
        default:
            return "?";
    }
}

void AppState::getParamRange(ParamType type, int& min, int& max, int& step) const {
    switch(type) {
        case ParamType::AMPLITUDE:
            min = 0; max = 100; step = 1;
            break;
        case ParamType::PULSE_WIDTH:
            min = 50; max = 400; step = 10;
            break;
        case ParamType::RATE:
            min = 10; max = 100; step = 1;
            break;
        case ParamType::BURST_HZ:
            min = 5; max = 50; step = 1; // *0.1
            break;
        case ParamType::BURST_DUTY:
            min = 10; max = 50; step = 1;
            break;
    }
}

void AppState::applyConstraints(StimParams& params) const {
    params.amplitude = constrain(params.amplitude, 0, 100);
    params.pulseWidthUs = constrain(params.pulseWidthUs, 50, 400);
    params.rateHz = constrain(params.rateHz, 10, 100);
    params.burstHz = constrain(params.burstHz, 0.5f, 5.0f);
    params.burstDutyPercent = constrain(params.burstDutyPercent, 10, 50);
}