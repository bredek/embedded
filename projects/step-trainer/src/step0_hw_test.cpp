// Крок 0: перевірка заліза. Дві шини I2C, екран, датчик ADXL345, кнопки.
// Що побачиш: у Serial Monitor (115200) знайдені адреси, потім 10 разів на секунду X Y Z і |a| у g.
// На екрані ті самі цифри і стан кнопок. У спокої |a| ≈ 1.00.
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "pins.h"
#include "button.h"
#include "adxl345.h"

Adafruit_SSD1306 oled(128, 64, &Wire, -1);
Button s1(PIN_S1), s2(PIN_S2);
bool oledOk = false, adxlOk = false;
float gx = 0, gy = 0, gz = 0;
uint32_t tRead = 0, tDraw = 0, tPrint = 0;

void scanBus(TwoWire& bus, const char* name) {
  Serial.printf("Шина %s: ", name);
  int found = 0;
  for (uint8_t a = 1; a < 127; a++) {
    bus.beginTransmission(a);
    if (bus.endTransmission() == 0) { Serial.printf("0x%02X ", a); found++; }
  }
  Serial.println(found ? "" : "нічого не знайдено: перевір дроти SDA/SCL і живлення");
}

void setup() {
  Serial.begin(115200);
  delay(1500);                                  // час, щоб відкрити Serial Monitor
  Wire.begin(PIN_SDA, PIN_SCL, 400000);         // шина OLED (на C3 і ADXL345 теж)
#if !ADXL_SHARED_BUS
  Wire1.begin(PIN_ASDA, PIN_ASCL, 400000);      // S3: шина 2 для ADXL345
#endif
  Serial.println("=== Лаба 9, крок 0: перевірка заліза ===");
#if ADXL_SHARED_BUS
  scanBus(Wire, "I2C (чекаємо 0x3C і 0x53)");
#else
  scanBus(Wire, "1 (OLED, чекаємо 0x3C)");
  scanBus(Wire1, "2 (ADXL345, чекаємо 0x53)");
#endif
  oledOk = oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  if (oledOk) oled.setRotation(OLED_ROTATION);
  adxlOk = adxl::begin();
  Serial.println(adxlOk ? "ADXL345: відповів (DEVID = 0xE5)" : "ADXL345: НЕ відповів. Перевір CS на 3V3, SDO на GND, SDA/SCL (S3: GPIO6/7, C3: GPIO6/7)");
  s1.begin(); s2.begin();
}

void loop() {
  uint32_t now = millis();
  s1.poll(now); s2.poll(now);
  if (adxlOk && now - tRead >= 20) {            // забираємо все, що накопичилось у черзі датчика
    tRead = now;
    int n = adxl::available();
    for (int i = 0; i < n; i++) adxl::read(gx, gy, gz);
  }
  float mag = sqrtf(gx * gx + gy * gy + gz * gz);
  if (now - tPrint >= 100) {
    tPrint = now;
    Serial.printf("X=%+5.2f Y=%+5.2f Z=%+5.2f |a|=%4.2f  S1=%d S2=%d\n", gx, gy, gz, mag, s1.pressed, s2.pressed);
  }
  if (oledOk && now - tDraw >= 100) {
    tDraw = now;
    oled.clearDisplay();
    oled.setTextSize(1); oled.setTextColor(SSD1306_WHITE);
    oled.setCursor(0, 0);  oled.print("HW TEST");
    oled.setCursor(0, 12); oled.print(adxlOk ? "ADXL345 OK" : "ADXL345 NO!");
    char b[24];
    snprintf(b, sizeof b, "X %+5.2f", gx); oled.setCursor(0, 26); oled.print(b);
    snprintf(b, sizeof b, "Y %+5.2f", gy); oled.setCursor(0, 36); oled.print(b);
    snprintf(b, sizeof b, "Z %+5.2f", gz); oled.setCursor(0, 46); oled.print(b);
    snprintf(b, sizeof b, "|a|=%4.2f", mag); oled.setCursor(64, 26); oled.print(b);
    snprintf(b, sizeof b, "S1 %s", s1.pressed ? "ON" : "--"); oled.setCursor(64, 40); oled.print(b);
    snprintf(b, sizeof b, "S2 %s", s2.pressed ? "ON" : "--"); oled.setCursor(64, 52); oled.print(b);
    oled.display();
  }
}
