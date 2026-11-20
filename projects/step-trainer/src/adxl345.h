// Мінімальний драйвер ADXL345 по I2C (на S3 друга шина Wire1: GPIO6/7; на C3 Super Mini та сама шина Wire, що й екран) без сторонніх бібліотек: тільки регістри з даташита.
// Датчик сам складає відліки (100 за секунду) у чергу FIFO на 32 штуки: програма може "відволіктись"
// на малювання екрана на 30 мс і не загубить жодного відліку.
#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include "pins.h"

namespace adxl {
const uint8_t REG_DEVID = 0x00, REG_BW_RATE = 0x2C, REG_POWER_CTL = 0x2D, REG_DATA_FORMAT = 0x31;
const uint8_t REG_DATAX0 = 0x32, REG_FIFO_CTL = 0x38, REG_FIFO_STATUS = 0x39;
const float G_PER_LSB = 0.0039f;     // повна роздільність: 3,9 мг на одиницю на будь-якому діапазоні

inline void write8(uint8_t reg, uint8_t val) {
  ADXL_BUS.beginTransmission(ADXL_ADDR); ADXL_BUS.write(reg); ADXL_BUS.write(val); ADXL_BUS.endTransmission();
}
inline uint8_t read8(uint8_t reg) {
  ADXL_BUS.beginTransmission(ADXL_ADDR); ADXL_BUS.write(reg); ADXL_BUS.endTransmission(false);
  ADXL_BUS.requestFrom((int)ADXL_ADDR, 1);
  return ADXL_BUS.available() ? ADXL_BUS.read() : 0;
}
inline bool begin() {                     // false, якщо датчик не відповів «я ADXL345» (DEVID = 0xE5)
  if (read8(REG_DEVID) != 0xE5) return false;
  write8(REG_BW_RATE, 0x0A);              // 100 відліків за секунду
  write8(REG_DATA_FORMAT, 0x09);          // повна роздільність, діапазон ±4 g
  write8(REG_FIFO_CTL, 0x80);             // режим Stream: черга 32 відліки, нові витісняють найстаріші
  write8(REG_POWER_CTL, 0x08);            // вмикаємо вимірювання
  return true;
}
inline int available() { return read8(REG_FIFO_STATUS) & 0x3F; }    // скільки відліків у черзі
inline void read(float& x, float& y, float& z) {                    // забрати один відлік, у g
  ADXL_BUS.beginTransmission(ADXL_ADDR); ADXL_BUS.write(REG_DATAX0); ADXL_BUS.endTransmission(false);
  ADXL_BUS.requestFrom((int)ADXL_ADDR, 6);
  int16_t raw[3] = {0, 0, 0};
  for (int i = 0; i < 3; i++) { uint8_t lo = ADXL_BUS.read(); uint8_t hi = ADXL_BUS.read(); raw[i] = (int16_t)((hi << 8) | lo); }
  x = raw[0] * G_PER_LSB; y = raw[1] * G_PER_LSB; z = raw[2] * G_PER_LSB;
}
}  // namespace adxl

// ---- детектор кроків: довжина вектора прискорення не залежить від того, як лежить плата в кишені
struct StepDetector {
  float lp = 1.0f;          // згладжений сигнал (прибирає дрібний шум)
  float base = 1.0f;        // повільне середнє = «спокій» (= 1 g гравітація)
  float pk = 0;             // найвищий пік поточного поштовху
  float avgPeak = 0;        // типова висота піків останніх кроків (0 = ще не знаємо)
  bool armed = true;        // true: можна ловити наступний пік
  uint32_t lastStepMs = 0;
  bool primed = false;

  // повертає true, якщо цей відлік (час tMs) — новий крок
  bool update(float x, float y, float z, uint32_t tMs) {
    float m = sqrtf(x * x + y * y + z * z);
    if (!primed) { lp = base = m; primed = true; return false; }
    lp += 0.3f * (m - lp);
    base += 0.02f * (lp - base);
    float dyn = lp - base;
    if (dyn > pk) pk = dyn;
    if ((uint32_t)(tMs - lastStepMs) > 2000) avgPeak = 0;   // 2 с без кроків: забуваємо типову висоту піків
    if (dyn < STEP_OFF_G) {                       // поштовх закінчився
      if (!armed) avgPeak = (avgPeak <= 0) ? pk : 0.7f * avgPeak + 0.3f * pk;
      armed = true; pk = 0;
    }
    float thr = STEP_ON_G;                        // слабкі відскоки після удару п'ятки (менше 45 % від типового піку) не кроки
    if (0.45f * avgPeak > thr) thr = 0.45f * avgPeak;
    if (armed && dyn > thr && (uint32_t)(tMs - lastStepMs) >= STEP_MIN_MS) {
      armed = false; lastStepMs = tMs; return true;
    }
    return false;
  }
};
