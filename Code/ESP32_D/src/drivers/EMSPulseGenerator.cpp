#include <Arduino.h>

#include ".\app\pins.h"
#include "..\include\drivers\EMSPulseGenerator.h"

// ====== Настройки вывода для стимуляции ======
//static const int PWM1_CH      = 0;     // ledc канал
static const int PWM1_RES     = 10;    // 10 бит (0..1023)
//static const int PWM1_FREQ    = 144;  // несущая для ШИМ амплитуды



// 🔥 Конструктор с параметрами
EMSPulseGenerator::EMSPulseGenerator(uint8_t pwmChannel, 
                                     uint8_t outputPin,
                                     uint32_t pwmFreq, 
                                     uint8_t pwmResolution,
                                     uint8_t pwmDuty
                                    )
    : pwmChannel_(pwmChannel),
      outputPin_(outputPin),
      pwmFreq_(pwmFreq),
      pwmResolution_(pwmResolution),
      pwmDuty_(pwmDuty)
{
    // Вычисляем максимальное значение duty cycle
    maxDuty_ = (1 << pwmResolution_) - 1;  // 2^resolution - 1
    
    // Инициализация с вашими параметрами
    amp_ = 10;
    pwUs_ = 200;
    rateHz_ = 144;
    pulsesPerBurst_ = 26;
    pauseBetweenBurstsMs_ = 235;
    
    // Расчет производных параметров
    pulsePeriodUs_ = 1000000UL / rateHz_;  // 6944 мкс
    burstDurationUs_ = pulsesPerBurst_ * pulsePeriodUs_;  // 180556 мкс
    pauseDurationUs_ = pauseBetweenBurstsMs_ * 1000UL;    // 235000 мкс
    fullCycleUs_ = burstDurationUs_ + pauseDurationUs_;   // 415556 мкс
    
    //pwmDuty_ = map(amp_, 0, 100, 0, maxDuty_);
   // pwmDutypwmDuty_ = 70;
    
    Serial.printf("[EMS] Constructor: CH=%d, Pin=%d, Freq=%lu Hz, Res=%d bit (max duty=%d)\n",
                  pwmChannel_, outputPin_, pwmFreq_, pwmResolution_, maxDuty_);
}

EMSPulseGenerator::EMSPulseGenerator() {
    // Инициализация с вашими параметрами
    amp_ = 10;
    pwUs_ = 200;
    rateHz_ = 144;
    pulsesPerBurst_ = 26;
    pauseBetweenBurstsMs_ = 235;
    
    // Расчет производных параметров
    pulsePeriodUs_ = 1000000UL / rateHz_;  // 6944 мкс
    burstDurationUs_ = pulsesPerBurst_ * pulsePeriodUs_;  // 180556 мкс
    pauseDurationUs_ = pauseBetweenBurstsMs_ * 1000UL;    // 235000 мкс
    fullCycleUs_ = burstDurationUs_ + pauseDurationUs_;   // 415556 мкс
    
    //pwmDuty_ = map(amp_, 0, 100, 0, 1023);

   // pwmDuty_ = 6;
}

bool EMSPulseGenerator::begin() {

    // 🔥 Настройка LEDC для этого конкретного канала
    ledcSetup(pwmChannel_, pwmFreq_, pwmResolution_);
    ledcAttachPin(outputPin_, pwmChannel_);
    ledcWrite(pwmChannel_, 0);
    
    // Инициализация временных меток
    lastPulseTs_ = micros();
    burstStartTs_ = lastPulseTs_;
    cycleStartTs_ = lastPulseTs_;
    running_ = false;
    
    Serial.println("[EMS] Initialized with parameters:");
    Serial.printf("  Pulse rate: %d Hz\n", pwmFreq_);
    Serial.printf("  Pulse Duty: %d\n", pwmDuty_);

    return true;
}

void EMSPulseGenerator::start() {
    running_ = true;
    // Сброс таймеров для корректного старта
    uint32_t now = micros();
    lastPulseTs_ = now;
    burstStartTs_ = now;
    cycleStartTs_ = now;
    nextPulseTs_ = now;  // ✅ НОВОЕ: расчетное время следующего импульса
    pulseActive_ = false;
    pulseCountInBurst_ = 0;
    inBurst_ = true;
    
    Serial.printf("[EMS CH%d] ✅ Started on pin %d\n", pwmChannel_, outputPin_);
}

void EMSPulseGenerator::stop() {
    running_ = false;
    pulseActive_ = false;
    inBurst_ = false;
    
    // 🔥 Выключаем PWM этого канала
    ledcWrite(pwmChannel_, 0);
    
    Serial.printf("[EMS CH%d] ⛔ Stopped\n", pwmChannel_);
}

void EMSPulseGenerator::setParams(uint8_t amplitudePercent) {
    // Применяем только амплитуду и длительность импульса
    amp_   = constrain(amplitudePercent, 0, 100);
    //pwUs_  = constrain(pulseWidthUs, 50, 500);
    
    // Преобразование амплитуды в значение ШИМ (0..1023)
    pwmDuty_ = map(amp_, 0, 100, 0, (1 << PWM1_RES) - 1);
    if (amp_ <= 0) pwmDuty_ = 0;

    

    Serial.printf("income = %d , pwmDuty_ = %d ", amplitudePercent, pwmDuty_);
    // ❌ УБРАЛИ Serial.printf отсюда - он вызывается слишком часто!
}

void EMSPulseGenerator::update() {
    if (!running_) return;

    const uint32_t now = micros();
    
    // Определяем положение в полном цикле
    const uint32_t cycleElapsed = now - cycleStartTs_;
    
    // Проверка завершения полного цикла
    //fullCycleUs_ = 415556;     // 180556 + 235000 = 415556 мкс
    if (cycleElapsed >= fullCycleUs_) {
        // Начинаем новый цикл
        cycleStartTs_ = now;
        burstStartTs_ = now;
        nextPulseTs_ = now;
        pulseCountInBurst_ = 0;
        inBurst_ = true;
        pulseActive_ = false;
        ledcWrite(pwmChannel_, 0);
       // digitalWrite(PWM_STATE_PIN, LOW);
        
       // Serial.printf("[EMS] 🔄 New cycle at %lu µs\n", now);
        return;
    }
    
    // Определяем, находимся ли мы в пачке или в паузе
    const uint32_t burstElapsed = now - burstStartTs_;
    
    //burstDurationUs_ = 180556; // 26 × 6944 = 180556 мкс
    if (burstElapsed >= burstDurationUs_) {
        // Мы в паузе между пачками
        if (inBurst_) {
            inBurst_ = false;
            pulseActive_ = false;
            ledcWrite(pwmChannel_, 0);
            //digitalWrite(PWM_STATE_PIN, LOW);
           // Serial.printf("[EMS] 💤 Pause (sent %d pulses)\n", pulseCountInBurst_);
        }
        else{
          
        }
        return;
    }
    
    // Мы внутри пачки - проверяем количество импульсов
    // if (pulseCountInBurst_ >= pulsesPerBurst_) {
    //     // Достигли лимита импульсов в пачке
    //     if (inBurst_) {
    //         inBurst_ = false;
    //         pulseActive_ = false;
    //         ledcWrite(PWM1_CH, 0);
    //         //digitalWrite(PWM_STATE_PIN, LOW);
    //         //Serial.printf("[EMS] ✅ Burst complete (%d pulses)\n", pulseCountInBurst_);
    //     }
    //     else{
    //       //digitalWrite(PWM_STATE_PIN, HIGH);
          
    //     }
    //     return;
    // }
    
    // ✅ ИСПРАВЛЕННАЯ ЛОГИКА: Проверяем состояние импульса
    if (!pulseActive_) {
        // ✅ Ждем времени начала следующего импульса
        //if (now >= nextPulseTs_) {
            // ✅ НАЧИНАЕМ НОВЫЙ ИМПУЛЬС - ОДИН РАЗ!
            pulseActive_ = true;
            pulseCountInBurst_++;
            lastPulseTs_ = now;
            nextPulseTs_ += pulsePeriodUs_;  // Планируем следующий импульс
            
            // ✅ Включаем PWM - ОДИН РАЗ!
            ledcWrite(pwmChannel_, pwmDuty_);
            //digitalWrite(PWM_STATE_PIN, HIGH);

            //digitalWrite(PWM_STATE_PIN, !digitalRead(PWM_STATE_PIN));  // Toggle
            
            // Отладка первых 3 импульсов
            // if (pulseCountInBurst_ <= 3) {
            //     Serial.printf("[EMS] Pulse #%d START at %lu µs (next: %lu µs)\n", 
            //                   pulseCountInBurst_, now, nextPulseTs_);
            // }
        //}
        // ✅ Если now < nextPulseTs_, просто ждем - ничего не делаем
    } else {
        // ✅ ИМПУЛЬС УЖЕ АКТИВЕН - проверяем не пора ли его выключить
        const uint32_t sincePulse = now - lastPulseTs_;
        //if (sincePulse >= pwUs_) {
            // ✅ ВЫКЛЮЧАЕМ ИМПУЛЬС - ОДИН РАЗ!
        //    pulseActive_ = false;
        //    ledcWrite(PWM1_CH, 0);
            // digitalWrite для toggle не трогаем
            
            // Отладка первых 3 импульсов
            // if (pulseCountInBurst_ <= 3) {
            //     Serial.printf("[EMS] Pulse #%d END at %lu µs (duration: %lu µs)\n", 
            //                   pulseCountInBurst_, now, sincePulse);
           // }
       // }
        // ✅ Если sincePulse < pwUs_, импульс еще активен - ничего не делаем
    }
}