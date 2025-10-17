#include <Arduino.h>
#include <SPI.h>

// ===== ПИНЫ ПО ВАШЕЙ РАСПИНОВКЕ =====
// SPI линии TLC5925
static const int PIN_SPI_SCK   = 16;   // mikroBUS SCK -> ESP32-S3 GPIO
static const int PIN_SPI_MOSI  = 15;   // mikroBUS MOSI (SDI на TLC5925)
static const int PIN_SPI_MISO  = -1;   // mikroBUS MISO (SDO) — TLC5925 не читает, можно -1
static const int PIN_TLC_LE    = 7;    // mikroBUS CS — используется как LATCH (LE) TLC5925

// Если OE выведен — активный низ (0 = включено). Иначе оставьте -1
static const int PIN_TLC_OE    = -1;   // например 38, если у вас есть такой вывод; иначе -1

// Энкодер
static const int PIN_ENC_A     = 47;   // mikroBUS PWM  -> ENA (канал A)
static const int PIN_ENC_B     = 21;   // mikroBUS AN   -> ENB (канал B)
static const int PIN_ENC_SW    = 13;   // mikroBUS INT  -> SW (кнопка)

// ===== ПАРАМЕТРЫ =====
#define LED_CHANNELS 16

SPIClass spi = SPIClass(FSPI); // Для ESP32-S3 используйте нужный SPI хост (FSPI/HSPI)

// Счётчик позиции из ISR
volatile int32_t g_pos = 0;

// Анимации
enum Mode { MODE_RUNNER = 0, MODE_TAIL = 1 };
static Mode mode = MODE_RUNNER;
static int dir = 1; // направление анимации: 1 — вперёд, -1 — назад

// Скорости анимаций (мс на шаг)
static uint16_t anim_period_ms = 80;  // бегунок
static uint16_t tail_period_ms = 60;  // дорожка с хвостом
static int anim_tail_len = 4;         // длина хвоста (1..16)

// Дебаунс кнопки
unsigned long last_sw_ms = 0;
bool sw_state = false;

// ===== TLC5925 low-level =====
static inline void pulse_latch() {
  digitalWrite(PIN_TLC_LE, HIGH);
  // delayMicroseconds(1); // при необходимости
  digitalWrite(PIN_TLC_LE, LOW);
}

// Передать 16 бит MSB-first в TLC5925 через SPI и зафиксировать выходы импульсом LE
void tlc_shift16(uint16_t data) {
  uint8_t hi = (data >> 8) & 0xFF;
  uint8_t lo = data & 0xFF;

  spi.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0)); // 1 МГц достаточно
  spi.transfer(hi);
  spi.transfer(lo);
  spi.endTransaction();

  pulse_latch();
}

// Установить произвольную маску каналов (1 = LED вкл)
inline void tlc_set_mask(uint16_t mask) {
  tlc_shift16(mask);
}

// Включить один светодиод по индексу 0..15 (остальные выкл)
void tlc_set_single(int index) {
  index %= LED_CHANNELS;
  if (index < 0) index += LED_CHANNELS;
  uint16_t m = (uint16_t)1u << index;
  tlc_set_mask(m);
}

// ===== ENC ISR: прерывание по фронтам канала A, читаем B для направления =====
void IRAM_ATTR encA_isr() {
  int a = digitalRead(PIN_ENC_A);
  int b = digitalRead(PIN_ENC_B);
  if (a == b) ++g_pos;
  else        --g_pos;
}

// ===== Анимации =====
void anim_runner_step_dir() {
  static uint32_t last = 0;
  static int idx = 0;
  uint32_t now = millis();
  if (now - last < anim_period_ms) return;
  last = now;

  idx = (idx + dir + LED_CHANNELS) % LED_CHANNELS;
  tlc_set_mask((uint16_t)1u << idx);
}

void anim_tail_step_dir() {
  static uint32_t last = 0;
  static int head = 0;
  uint32_t now = millis();
  if (now - last < tail_period_ms) return;
  last = now;

  head = (head + dir + LED_CHANNELS) % LED_CHANNELS;

  uint16_t mask = 0;
  for (int i = 0; i < anim_tail_len; ++i) {
    int idx = head - i*dir; // хвост «за» головой по направлению
    idx %= LED_CHANNELS;
    if (idx < 0) idx += LED_CHANNELS;
    mask |= (uint16_t)1u << idx;
  }
  tlc_set_mask(mask);
}

void setup() {
  Serial.begin(115200);
  delay(50);

  // LATCH (CS на mikroBUS), OE (если есть)
  pinMode(PIN_TLC_LE, OUTPUT);
  digitalWrite(PIN_TLC_LE, LOW);

  if (PIN_TLC_OE >= 0) {
    pinMode(PIN_TLC_OE, OUTPUT);
    digitalWrite(PIN_TLC_OE, LOW); // активный низ: 0 = включено
  }

  // Инициализация SPI (без аппаратного SS — LATCH дергаем вручную)
  spi.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI, -1);

  // Погасить все
  tlc_set_mask(0x0000);

  // Энкодер: входы с pullup
  pinMode(PIN_ENC_A, INPUT_PULLUP);
  pinMode(PIN_ENC_B, INPUT_PULLUP);
  pinMode(PIN_ENC_SW, INPUT_PULLUP);

  // Прерывание на A по любому изменению
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_A), encA_isr, CHANGE);

  // Стартовое состояние кольца
  tlc_set_single(0);

  Serial.println("Rotary O 2 Click demo: RUNNER and TAIL modes. Press SW to switch mode.");
}

void loop() {
  // 1) Управление скоростью/направлением энкодером
  static int32_t last_pos = 0;
  int32_t pos = g_pos;
  if (pos != last_pos) {
    int32_t d = pos - last_pos;
    last_pos = pos;

    // Пример: поворот меняет скорость (вправо — быстрее, влево — медленнее)
    int delta = (d > 0) ? 1 : -1;
    anim_period_ms = constrain((int)anim_period_ms - delta*5, 10, 400);
    tail_period_ms = constrain((int)tail_period_ms - delta*5, 10, 400);

    // Альтернатива: менять направление (раскомментируйте ниже)
    // dir = (d > 0) ? 1 : -1;

    Serial.printf("encoder_pos=%ld, runner=%u ms, tail=%u ms\n",
                  (long)pos, anim_period_ms, tail_period_ms);
  }

  // 2) Переключение режима по нажатию SW (краткое нажатие)
  bool sw_now = (digitalRead(PIN_ENC_SW) == LOW); // активный LOW
  unsigned long ms = millis();
  if (sw_now != sw_state && (ms - last_sw_ms) > 10) { // антидребезг ~10 мс
    sw_state = sw_now;
    last_sw_ms = ms;
    if (sw_state) { // нажатие
      mode = (mode == MODE_RUNNER) ? MODE_TAIL : MODE_RUNNER;
      Serial.printf("Mode switched: %s\n", mode == MODE_RUNNER ? "RUNNER" : "TAIL");
    }
  }

  // 3) Выполнить выбранную анимацию
  if (mode == MODE_RUNNER) anim_runner_step_dir();
  else                     anim_tail_step_dir();

  delay(1);
}