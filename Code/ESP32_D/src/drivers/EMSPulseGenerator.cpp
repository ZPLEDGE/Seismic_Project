#include <Arduino.h>

#include ".\app\pins.h"
#include "..\include\drivers\EMSPulseGenerator.h"


// ====== Настройки вывода для стимуляции ======
//static const int PWM_CH_1_PIN      = 17;    // GPIO для управления драйвером/транзисторами
static const int PWM1_CH      = 0;     // ledc канал
static const int PWM1_RES     = 10;    // 10 бит (0..1023)
static const int PWM1_FREQ    = 1250; // несущая для ШИМ амплитуды (20 кГц)

EMSPulseGenerator::EMSPulseGenerator() {
  // Конструктор - инициализация значений по умолчанию уже выполнена в заголовке
}

bool EMSPulseGenerator::begin() {
  // Настройка LEDC для ШИМ
  ledcSetup(PWM1_CH, PWM1_FREQ, PWM1_RES);
  ledcAttachPin(PWM_CH_1_PIN, PWM1_CH);
  ledcWrite(PWM1_CH, 0);
  
  // Инициализация временных меток
  lastPulseTs_ = micros();
  lastBurstTs_ = lastPulseTs_;
  running_ = false;
  
  return true;
}

void EMSPulseGenerator::start() {
  running_ = true;
  // Сброс таймеров для корректного старта
  lastPulseTs_ = micros();
  lastBurstTs_ = lastPulseTs_;
  pulseActive_ = false;
}

void EMSPulseGenerator::stop() {
  running_ = false;
  pulseActive_ = false;
  ledcWrite(PWM1_CH, 0); // Выключаем ШИМ
}

void EMSPulseGenerator::setParams(uint8_t amplitudePercent,
                                  uint16_t pulseWidthUs,
                                  uint8_t rateHz,
                                  float burstHz,
                                  uint8_t burstDutyPercent) {
  // Применение безопасных ограничений
  amp_   = constrain(amplitudePercent, 0, 100);   // до 50% для безопасности
  pwUs_  = constrain(pulseWidthUs, 50, 400);     // 50..400 мкс
  rateHz_= (uint8_t)constrain(rateHz, 10, 100);  // 10..100 Гц
  bHz_   = constrain(burstHz, 0.5f, 5.0f);       // 0.5..5 Гц
  bDuty_ = constrain(burstDutyPercent, 10, 50);  // 10..50%

  // Предвычисление производных параметров
  if (rateHz_ < 1) rateHz_ = 1;
  pulsePeriodUs_ = (uint32_t)(1000000UL / rateHz_);
  
  // Гарантируем паузу между импульсами
  if (pulsePeriodUs_ <= pwUs_) {
    pulsePeriodUs_ = pwUs_ + 100; // +100 мкс минимальный зазор
  }

  burstPeriodUs_ = (uint32_t)(1000000.0f / bHz_);
  burstOnUs_ = (uint32_t)(burstPeriodUs_ * (bDuty_ / 100.0f));
  
  // Преобразование амплитуды в значение ШИМ (0..1023)
  ampDuty_ = map(amp_, 0, 100, 0, (1 << PWM1_RES) - 1);
  if (amp_ <= 0) ampDuty_ = 0;

  
}



void EMSPulseGenerator::update() {
  // if (!running_) return;

  // const uint32_t now = micros();

  // // Управление циклом пачек
  // const uint32_t burstElapsed = now - lastBurstTs_;
  // const bool inBurstOn = (burstElapsed < burstOnUs_);

  // // Проверка завершения цикла пачки
  // if (burstElapsed >= burstPeriodUs_) {
  //   // Начинаем новый цикл пачки
  //   lastBurstTs_ = now;
  //   pulseActive_ = false;
  //   ledcWrite(PWM1_CH, 0);
  // }

  // if (!inBurstOn) {
  //   // Пауза между пачками - ШИМ выключен
  //   pulseActive_ = false;
  //   ledcWrite(PWM1_CH, 0);
  //   return;
  // }

  // // Внутри активной части пачки: генерируем импульсы
  // const uint32_t sincePulse = now - lastPulseTs_;
  
  // if (!pulseActive_) {    
  //   // Проверяем, можно ли начать новый импульс
  //   if (sincePulse >= pulsePeriodUs_) {
  //     digitalWrite(PWM_STATE_PIN, true);
  //     // Начинаем новый импульс
  //     lastPulseTs_ = now;
  //     pulseActive_ = true;
  //     ledcWrite(PWM1_CH, ampDuty_); // Включаем ШИМ с заданной амплитудой
  //   }
  // } else {
  //   // Импульс активен - проверяем, не пора ли его выключить
  //   if (sincePulse >= pwUs_) {
  //     pulseActive_ = false;
  //     ledcWrite(PWM1_CH, 0); // Выключаем ШИМ до следующего импульса
  //   }
  //   digitalWrite(PWM_STATE_PIN, false);
  // }





//Test
// Ограничиваем значение от 0 до 100%
  amp_ = constrain(amp_, 0, 100);
  // Конвертируем проценты в значение duty cycle (0-1023 для 10-бит)
  int dutyCycle = map(amp_, 0, 100, 0, 1023);

  ledcWrite(PWM1_CH, dutyCycle);

  //Serial.print("% (duty cycle: ");

}