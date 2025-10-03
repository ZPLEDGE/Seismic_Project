#include <Arduino.h>

#include "drivers/EncoderEC12.h"
#include "drivers/EMSPulseGenerator.h"

#include "app/pins.h"

// #define DT_PIN 26
// #define CLK_PIN 27


// Создаём два независимых экземпляра (порядок: CLK, DT)
static EncoderEC12 encoderA(ENC_A_CLK_PIN, ENC_A_DT_PIN, /*debounceUs*/ 1000);
EMSPulseGenerator stim;


//Encoder Propreties
static volatile int32_t posA = 0;
constexpr int STEPS_PER_REV = 20;

// PWD Propreties
enum class ParamSel { AMP, WIDTH, RATE, BDUTY, BHz };
ParamSel selected = ParamSel::AMP;
// Текущие параметры (начальные значения)
uint8_t  g_amp = 10;     // %
uint16_t g_pw  = 200;    // мкс
uint8_t  g_rate= 20;     // Гц
float    g_bhz = 1.0f;   // Гц
uint8_t  g_bdy = 20;     // %


static void applyParams() {
  stim.setParams(g_amp, g_pw, g_rate, g_bhz, g_bdy);
}




static float wrapDegrees(float degrees) {
    // Нормализация в [0..360)
    while (degrees < 0)   degrees += 360.0f;
    while (degrees >= 360.0f) degrees -= 360.0f;
    return degrees;
}
void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("Serial Init - Ok...");
  // pinMode(CLK_PIN, INPUT_PULLUP);
  // pinMode(DT_PIN, INPUT_PULLUP);

  // attachInterrupt(digitalPinToInterrupt(CLK_PIN), readEncoder, CHANGE);

  // pinMode(STATE_LED_PIN, OUTPUT);        // LED как выход
  // digitalWrite(STATE_LED_PIN, LOW);      // Начальное состояние - выключен

  pinMode(PWM_STATE_PIN, OUTPUT);        // LED как выход
  digitalWrite(PWM_STATE_PIN, LOW);      // Начальное состояние - выключен
 
// EMS generator
  if (!stim.begin()) {
    Serial.println("ERROR: Failed to initialize EMS generator!");
    return;
  }
  else{
    Serial.println("EMS generator Init - Ok...");
  }

  applyParams();
  stim.start();
  
//-------------------

// Encoder

  encoderA.onStep([](int8_t delta){
      posA += delta;
      const float deg = wrapDegrees((posA * 360.0f) / STEPS_PER_REV);
      Serial.printf("[A] pos=%ld  deg=%.1f°  (Δ=%d)\n", posA, deg, delta);



      g_amp = (uint8_t)constrain((int)g_amp + delta, 0, 100);

      Serial.printf("[A] pos=%ld  deg=%.1f°  (Δ=%d) g_amp=%ld\n", posA, deg, delta, g_amp);
      applyParams();

  });

  if (!encoderA.begin()) {
    Serial.println("ERROR: Failed to initialize encoder!");
    return;
  }else{
    Serial.println("Encoder Init - Ok...");
  }


}

void loop() {
  //  static int lastPos = 0;
  //  if (lastPos < 10) {

  //   lastPos++;
  //  }
  //  lastPos = 30;
  encoderA.update();
  stim.update();

  //   // Предположим 20 шагов = 360°
  //   float degrees = (encoderPosition * 360.0) / 20.0;

  //   while (degrees < 0) degrees += 360;
  //   while (degrees >= 360) degrees -= 360;

  //   Serial.printf("Позиция: %d | Угол: %.1f°\n", encoderPosition, degrees);
  // }
}