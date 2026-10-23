#pragma once

#include <Arduino.h>

// LED pins
static constexpr uint8_t PIN_LED_RED = 15;    // IO14
static constexpr uint8_t PIN_LED_GREEN = 16;   // IO1

// Button pins (wired to GND, using internal pull-ups)
static constexpr uint8_t PIN_BTN_NEXT = 4;    // IO2
static constexpr uint8_t PIN_BTN_PREV = 10;   // IO42
