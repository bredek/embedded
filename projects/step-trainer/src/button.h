// Кнопка з придушенням брязкоту і довгим натисканням, без delay().
#pragma once
#include <Arduino.h>
#include "pins.h"

struct Button {
  int pin;
  bool raw = false, pressed = false, longSent = false;
  uint32_t changedAt = 0, pressedAt = 0;
  Button(int p) : pin(p) {}
  void begin() { pinMode(pin, INPUT_PULLUP); }
  // 0 = нічого, 1 = коротке (спрацьовує при відпусканні), 2 = довге (спрацьовує через LONG_MS, поки кнопку ще тримають)
  int poll(uint32_t now) {
    bool r = (digitalRead(pin) == LOW);
    if (r != raw) { raw = r; changedAt = now; }
    if ((uint32_t)(now - changedAt) >= DEBOUNCE_MS && raw != pressed) {
      pressed = raw;
      if (pressed) { pressedAt = now; longSent = false; }
      else if (!longSent) return 1;
    }
    if (pressed && !longSent && (uint32_t)(now - pressedAt) >= LONG_MS) { longSent = true; return 2; }
    return 0;
  }
};
