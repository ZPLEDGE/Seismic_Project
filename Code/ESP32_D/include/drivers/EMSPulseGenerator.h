#pragma once
#include <Arduino.h>

#include  ".\core\IStimGenerator.h"

class EMSPulseGenerator : public IStimGenerator {
public:

    //using PulseCallback = std::function<void(uint16_t pulseNumber, uint32_t timestamp)>;
    //void onPulseEnd(PulseCallback callback) { pulseEndCallback_ = callback; }

        // 🔥 Конструктор с параметрами PWM канала, частоты и разрешения
    EMSPulseGenerator(uint8_t pwmChannel, 
                     uint8_t outputPin,
                     uint32_t pwmFreq = 144, 
                     uint8_t pwmResolution = 10,
                    uint8_t  pwmDuty = 70);

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

    // 🔥 PWM параметры (уникальные для каждого экземпляра)
    uint8_t  pwmChannel_;      // LEDC канал (0-15)
    uint8_t  outputPin_;       // GPIO пин
    uint32_t pwmFreq_;         // Частота PWM (Гц)
    uint8_t  pwmResolution_;   // Разрешение PWM (бит)
    uint16_t maxDuty_;         // Максимальное значение duty cycle (2^pwmResolution - 1)


    uint8_t  amp_ = 10;          // % (начальная амплитуда 10%)
    uint16_t pwUs_ = 200;        // микросек (длительность импульса)
    uint8_t  rateHz_ = 144;      // Гц (частота импульсов в пачке)
    uint8_t  pulsesPerBurst_ = 26;  // количество импульсов в пачке
    uint32_t pauseBetweenBurstsMs_ = 235;  // пауза между пачками в мс


    // Производные параметры
    uint32_t pulsePeriodUs_ = 6944;     // 1/144 Гц = 6944 мкс
    uint32_t burstDurationUs_ = 180556; // 26 × 6944 = 180556 мкс
    uint32_t pauseDurationUs_ = 235000; // 235 мс = 235000 мкс
    uint32_t fullCycleUs_ = 415556;     // 180556 + 235000 = 415556 мкс
    uint16_t pwmDuty_ = 0;              // 0..1023

    // Состояние
    bool     running_ = false;
    bool     pulseActive_ = false;
    uint32_t lastPulseTs_ = 0;
    uint32_t nextPulseTs_ = 0;
    uint32_t burstStartTs_ = 0;
    uint32_t cycleStartTs_ = 0;
    uint16_t pulseCountInBurst_ = 0;
    bool     inBurst_ = false;
};

