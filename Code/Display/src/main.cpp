#include <Arduino.h>
#define LGFX_USE_V1
#include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Device
{
//  lgfx::Panel_NT35510
  //lgfx::Panel_RGB  _panel_instance;
  lgfx::Panel_ILI9481 _panel_instance;
  lgfx::Bus_SPI _bus_instance;

public:
  LGFX(void)
  {
    {
      auto cfg = _bus_instance.config();
      cfg.spi_host = SPI2_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 40000000;  // Попробуем максимум - 40MHz
      cfg.freq_read  = 16000000;
      cfg.spi_3wire  = false;
      cfg.use_lock   = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk = 12;
      cfg.pin_mosi = 11;
      cfg.pin_miso = 13;
      cfg.pin_dc   = 4;
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }

    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs           =    5;
      cfg.pin_rst          =    2;
      cfg.pin_busy         =   -1;
      cfg.panel_width      =  320;
      cfg.panel_height     =  480;
      cfg.offset_x         =    0;
      cfg.offset_y         =    0;
      cfg.offset_rotation  =    0;
      cfg.dummy_read_pixel =    8;
      cfg.dummy_read_bits  =    1;
      cfg.readable         = true;
      cfg.invert           = false;
      cfg.rgb_order        = false;
      cfg.dlen_16bit       = false;
      cfg.bus_shared       = true;
      _panel_instance.config(cfg);
    }
    setPanel(&_panel_instance);
  }
};

LGFX tft;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== ILI9481 Performance Test ===\n");

  tft.init();
  tft.setRotation(1);  // 480x320
  
  int width = tft.width();
  int height = tft.height();
  
  Serial.printf("Display: %dx%d\n", width, height);
  Serial.printf("SPI Frequency: 40 MHz\n\n");

  // Тест 1: Заливка всего экрана
  Serial.println("Test 1: Full screen fill");
  unsigned long start = millis();
  for (int i = 0; i < 10; i++) {
    tft.fillScreen(TFT_RED);
    tft.fillScreen(TFT_GREEN);
    tft.fillScreen(TFT_BLUE);
  }
  unsigned long elapsed = millis() - start;
  float fps_fullscreen = 30000.0 / elapsed;
  Serial.printf("Time: %lu ms (30 fills)\n", elapsed);
  Serial.printf("FPS (full screen): %.2f\n\n", fps_fullscreen);

  // Тест 2: Рисование прямоугольников
  Serial.println("Test 2: Drawing rectangles");
  start = millis();
  for (int i = 0; i < 100; i++) {
    tft.fillRect(random(width), random(height), 50, 50, random(0xFFFF));
  }
  elapsed = millis() - start;
  Serial.printf("Time: %lu ms (100 rectangles)\n", elapsed);
  Serial.printf("Rectangles per second: %.2f\n\n", 100000.0 / elapsed);

  // Тест 3: Рисование пикселей
  Serial.println("Test 3: Drawing pixels");
  start = millis();
  for (int i = 0; i < 10000; i++) {
    tft.drawPixel(random(width), random(height), random(0xFFFF));
  }
  elapsed = millis() - start;
  Serial.printf("Time: %lu ms (10000 pixels)\n", elapsed);
  Serial.printf("Pixels per second: %.2f\n\n", 10000000.0 / elapsed);

  // Тест 4: Рисование линий
  Serial.println("Test 4: Drawing lines");
  tft.fillScreen(TFT_BLACK);
  start = millis();
  for (int i = 0; i < 100; i++) {
    tft.drawLine(random(width), random(height), random(width), random(height), random(0xFFFF));
  }
  elapsed = millis() - start;
  Serial.printf("Time: %lu ms (100 lines)\n", elapsed);
  Serial.printf("Lines per second: %.2f\n\n", 100000.0 / elapsed);

  // Тест 5: Текст
  Serial.println("Test 5: Drawing text");
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(2);
  start = millis();
  for (int i = 0; i < 50; i++) {
    tft.setCursor(10, 10);
    tft.setTextColor(random(0xFFFF));
    tft.printf("Frame: %d", i);
  }
  elapsed = millis() - start;
  Serial.printf("Time: %lu ms (50 text updates)\n", elapsed);
  Serial.printf("Text updates per second: %.2f\n\n", 50000.0 / elapsed);

  // Тест 6: Анимация (движущийся квадрат)
  Serial.println("Test 6: Animation test (moving square)");
  tft.fillScreen(TFT_BLACK);
  start = millis();
  int frames = 0;
  int x = 0, y = 160;
  int dx = 5;
  
  while (millis() - start < 5000) {  // 5 секунд
    // Стираем старый квадрат
    tft.fillRect(x, y, 50, 50, TFT_BLACK);
    
    // Двигаем
    x += dx;
    if (x > width - 50 || x < 0) dx = -dx;
    
    // Рисуем новый квадрат
    tft.fillRect(x, y, 50, 50, TFT_WHITE);
    
    frames++;
  }
  elapsed = millis() - start;
  float fps_animation = frames * 1000.0 / elapsed;
  Serial.printf("Frames: %d in %lu ms\n", frames, elapsed);
  Serial.printf("FPS (animation): %.2f\n\n", fps_animation);

  // Итоговая информация
  Serial.println("=== Summary ===");
  Serial.printf("Full screen FPS: %.2f\n", fps_fullscreen);
  Serial.printf("Animation FPS: %.2f\n", fps_animation);
  Serial.printf("Display resolution: %dx%d (%d pixels)\n", width, height, width * height);
  
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(3);
  tft.setCursor(100, 140);
  tft.printf("FPS: %.1f", fps_animation);
}

void loop() {
  // Пусто
}