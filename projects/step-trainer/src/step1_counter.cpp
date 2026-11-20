// Крок 1: лічильник кроків. Датчик ADXL345 + екран + кнопка S1.
// S1 коротко: пауза / старт. S1 довго (1 с): скинути лічильник.
// Програма стартує одразу в режимі РАХУЮ: кинув у кишеню і пішов.
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "pins.h"
#include "button.h"
#include "adxl345.h"

Adafruit_SSD1306 oled(128, 64, &Wire, -1);
Button s1(PIN_S1);
StepDetector det;
bool oledOk = false, adxlOk = false, running = true;
uint32_t steps = 0, tPoll = 0, tDraw = 0;

void pollSteps(uint32_t now) {
  int n = adxl::available();
  if (n > 32) n = 32;
  for (int i = 0; i < n; i++) {
    float x, y, z;
    adxl::read(x, y, z);
    uint32_t t = now - (uint32_t)(n - 1 - i) * 10;       // відліки йдуть через 10 мс (100 Гц)
    if (det.update(x, y, z, t) && running) steps += STEPS_PER_PEAK;
  }
}

void draw() {
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(1); oled.setCursor(0, 0); oled.print(running ? "RUN" : "PAUSE");
  if (!adxlOk) { oled.setCursor(64, 0); oled.print("NO ADXL!"); }
  char b[16];
  snprintf(b, sizeof b, "%lu", (unsigned long)steps);
  oled.setTextSize(3); oled.setCursor(0, 20); oled.print(b);
  oled.setTextSize(1); oled.setCursor(0, 56); oled.print("steps");
  oled.display();
}

void setup() {
  Serial.begin(115200);
  Wire.begin(PIN_SDA, PIN_SCL, 400000);
#if !ADXL_SHARED_BUS
  Wire1.begin(PIN_ASDA, PIN_ASCL, 400000);      // S3: окрема шина для ADXL345
#endif
  oledOk = oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  if (oledOk) oled.setRotation(OLED_ROTATION);
  adxlOk = adxl::begin();
  s1.begin();
}

void loop() {
  uint32_t now = millis();
  int e = s1.poll(now);
  if (e == 1) running = !running;
  if (e == 2) steps = 0;
  if (adxlOk && now - tPoll >= 20) { tPoll = now; pollSteps(now); }
  if (oledOk && now - tDraw >= 200) { tDraw = now; draw(); }
}
