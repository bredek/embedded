#include <Arduino.h>
#include "config.h"

int mode = 1;

void checkButtons() {
    static bool prevNext = HIGH;
    static bool prevPrev = HIGH;

    bool nextNow = digitalRead(PIN_BTN_NEXT);
    bool prevNow = digitalRead(PIN_BTN_PREV);

    // Button 1 (IO2): normal press -> mode 1
    if (nextNow == LOW && prevNext == HIGH) {
        mode = 1;
        Serial.println("Mode changed to 1 (BTN1)");
    }

    // Button 2 (IO42): normal press -> mode 2
    if (prevNow == LOW && prevPrev == HIGH) {
        mode = 2;
        Serial.println("Mode changed to 2 (BTN2)");
    }

    prevNext = nextNow;
    prevPrev = prevNow;
}

void waitWithButtonCheck(uint32_t totalMs) {
    const uint32_t chunkMs = 10;
    uint32_t elapsedMs = 0;

    while (elapsedMs < totalMs) {
        checkButtons();
        const uint32_t remainingMs = totalMs - elapsedMs;
        const uint32_t thisDelayMs = (remainingMs < chunkMs) ? remainingMs : chunkMs;
        delay(thisDelayMs);
        elapsedMs += thisDelayMs;
    }
}

void setup() {
    pinMode(PIN_LED_RED, OUTPUT);
    pinMode(PIN_LED_GREEN, OUTPUT);

    pinMode(PIN_BTN_NEXT, INPUT_PULLUP); // Button 1 (IO2)
    pinMode(PIN_BTN_PREV, INPUT_PULLUP); // Button 2 (IO42)

    Serial.begin(115200);
    Serial.println("Started. Mode 1");
}

void loop() {
    checkButtons();

    if (mode == 1) {
        // Mode 1: both LEDs on 200ms, both off 200ms
        digitalWrite(PIN_LED_RED, HIGH);
        digitalWrite(PIN_LED_GREEN, HIGH);
        waitWithButtonCheck(200);

        digitalWrite(PIN_LED_RED, LOW);
        digitalWrite(PIN_LED_GREEN, LOW);
        waitWithButtonCheck(200);
    } else {
        // Mode 2: LED1 on LED2 off 1000ms, then LED1 off LED2 on 1000ms
        digitalWrite(PIN_LED_RED, HIGH);
        digitalWrite(PIN_LED_GREEN, LOW);
        waitWithButtonCheck(1000);

        digitalWrite(PIN_LED_RED, LOW);
        digitalWrite(PIN_LED_GREEN, HIGH);
        waitWithButtonCheck(1000);
    }
}