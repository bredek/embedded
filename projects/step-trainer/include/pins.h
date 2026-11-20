// Лаба 9 (версія з ADXL345, без зумера): піни і налаштування для всіх програм.
// Звір з docs/breadboard-v1-realistic.svg і з написами на СВОЇЙ платі.
#pragma once
#include <Arduino.h>

// ---- Яка плата? За замовчуванням ESP32-S3 DevKitC-1. Для ESP32-C3 Super Mini збирай env:c3_step0 / c3_step1 / c3_step2
//      (вони додають -DBOARD_C3). Усе, що залежить від плати, лише в цьому блоці.
#ifdef BOARD_C3
// ---- піни ESP32-C3 Super Mini (одна шина I2C: OLED і ADXL345 паралельно на тих самих двох дротах)
const int PIN_SDA  = 5;    // I2C: SDA (OLED і ADXL345)
const int PIN_SCL  = 6;    // I2C: SCL (OLED і ADXL345)
const int PIN_S1   = 7;    // кнопка S1 «СТАРТ»: друга ніжка на GND
const int PIN_S2   = 10;   // кнопка S2 «ЦІЛЬ»: друга ніжка на GND
const int PIN_VBAT = 4;    // середина дільника 100к + 100к від акумулятора (АЦП1, GPIO0…4)
#define ADXL_BUS Wire      // у C3 один I2C-контролер: датчик на тій самій шині, що й екран
#define ADXL_SHARED_BUS 1
#else
// ---- піни ESP32-S3 DevKitC-1
const int PIN_SDA  = 8;    // I2C шина 1 (Wire):  OLED SDA
const int PIN_SCL  = 9;    // I2C шина 1 (Wire):  OLED SCL
const int PIN_ASDA = 6;    // I2C шина 2 (Wire1): ADXL345 SDA
const int PIN_ASCL = 7;    // I2C шина 2 (Wire1): ADXL345 SCL
const int PIN_S1   = 1;    // кнопка S1 «СТАРТ»: друга ніжка на GND
const int PIN_S2   = 2;    // кнопка S2 «РЕЖИМ»: друга ніжка на GND
const int PIN_VBAT = 4;    // середина дільника 100к + 100к від акумулятора (АЦП1). На макетці дільника немає
#define ADXL_BUS Wire1
#define ADXL_SHARED_BUS 0
#endif

#define HAS_VBAT 1         // 1 = на платі є дільник R1/R2 (перфоплата), 0 = немає (макетка)

// ---- I2C-адреси
const uint8_t OLED_ADDR  = 0x3C;   // буває 0x3D: подивись, що знайде step0
const uint8_t ADXL_ADDR  = 0x53;   // SDO на GND = 0x53, SDO на VCC = 0x1D
const uint8_t OLED_ROTATION = 2;   // 2 = перевернути на 180° (OLED лежить штирями до плати); 0 = звичайно

// ---- детектор кроків (ADXL345 міряє прискорення; у спокої сумарно 1,00 g)
const float    STEP_ON_G   = 0.15f;   // крок = пік вище за «спокій» на 0,15 g (у кишені джинсів ходьба дає 0,3…1 g)
const float    STEP_OFF_G  = 0.06f;   // нового кроку не чекаємо, поки сигнал не впаде нижче 0,06 g (гістерезис)
const uint32_t STEP_MIN_MS = 300;     // «вікно тиші»: не частіше 200 кроків/хв
const uint8_t  STEPS_PER_PEAK = 1;    // 1 = один пік на крок; 2, якщо рахує лише кожен другий (див. тест 8 у README)

// ---- кнопки
const uint32_t DEBOUNCE_MS = 30;      // стан має протриматись 30 мс
const uint32_t LONG_MS = 1000;        // довге натискання = 1 с

// ---- екран і акумулятор
const uint32_t SCREEN_OFF_MS = 20000; // екран гасне через 20 с без кнопок (кроки рахуються далі)
const float    VBAT_DIV = 2.0f;       // дільник 100к + 100к ділить навпіл
