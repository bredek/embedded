#include <Arduino.h>

const int PIN_LDR   = 4;     // GPIO4 = ADC1_CH3, середина дільника R1 (фоторезистор) + R2 10 кОм
const int PIN_RELAY = 21;    // GPIO21 -> R3 10 кОм -> база VT1 BC547B

const int THRESHOLD_DARK  = 1900;   // ADC нижче -> темно -> вмикаємо. Підбери під свою кімнату (крок 1)
const int THRESHOLD_LIGHT = 2400;   // ADC вище -> світло -> вимикаємо. Має бути БІЛЬШЕ за THRESHOLD_DARK

bool relayOn = false;        // пам'ять: що ми зараз подаємо на реле

void setup() {
  Serial.begin(115200);
  pinMode(PIN_RELAY, OUTPUT);
  digitalWrite(PIN_RELAY, LOW);      // на старті реле вимкнене
  analogReadResolution(12);          // 0...4095
}

void loop() {
  // 1. Зчитати значення АЦП
  int adc = analogRead(PIN_LDR);

  // 2. Порівняти з порогами (гістерезис)
  if (adc < THRESHOLD_DARK) {
    relayOn = true;                  // темно
  } else if (adc > THRESHOLD_LIGHT) {
    relayOn = false;                 // світло
  }
  // між порогами: relayOn залишається, яким був

  // 3. Подати рівень на GPIO Out
  digitalWrite(PIN_RELAY, relayOn ? HIGH : LOW);

  Serial.print("ADC=");
  Serial.print(adc);
  Serial.print("  relay=");
  Serial.println(relayOn ? "ON" : "OFF");
  delay(200);
}