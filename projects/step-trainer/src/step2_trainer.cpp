// Крок 2: повний тренер кроків для кишені. ADXL345 + OLED + S1 + S2 + (на перфоплаті) вимірювання акумулятора.
// Екран: RUN/PAUSE і заряд, великі кроки, ціль і смужка, внизу темп (кроків/хв) і час.
// S1 коротко: пауза / старт. S1 довго: скинути все.  S2 коротко: наступна ціль. S2 довго: погасити екран.
// Екран сам гасне через 20 с (кроки рахуються далі). Будь-яка кнопка його будить. Досягнув цілі: екран вмикається і блимає GOAL!
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "pins.h"
#include "button.h"
#include "adxl345.h"

Adafruit_SSD1306 oled(128, 64, &Wire, -1);
Button s1(PIN_S1), s2(PIN_S2);
StepDetector det;

const uint32_t GOALS[] = {50, 100, 500, 1000, 2000, 5000, 10000};
const int NGOALS = sizeof(GOALS) / sizeof(GOALS[0]);
int goalIdx = 3;                                   // старт: 1000 кроків

bool oledOk = false, adxlOk = false, running = true, screenOn = true, goalShown = false;
uint32_t steps = 0, runMs = 0, lastTick = 0;
uint32_t tPoll = 0, tDraw = 0, tBat = 0, lastKey = 0, flashUntil = 0;
int batPct = -1;                                   // -1 = невідомо / живлення від USB

// темп із 6 останніх кроків: (n-1) інтервалів за (найновіший - найстаріший) мс
uint32_t stepT[6];
int nT = 0;
void pushStep(uint32_t t) {
  if (nT < 6) stepT[nT++] = t;
  else { for (int i = 0; i < 5; i++) stepT[i] = stepT[i + 1]; stepT[5] = t; }
}
uint32_t cadence(uint32_t now) {
  if (nT < 2 || now - stepT[nT - 1] > 3000) return 0;
  return (uint32_t)(nT - 1) * 60000UL / (stepT[nT - 1] - stepT[0]);
}

void pollSteps(uint32_t now) {
  int n = adxl::available();
  if (n > 32) n = 32;
  for (int i = 0; i < n; i++) {
    float x, y, z;
    adxl::read(x, y, z);
    uint32_t t = now - (uint32_t)(n - 1 - i) * 10;
    if (det.update(x, y, z, t) && running) {
      steps += STEPS_PER_PEAK;
      pushStep(t);
      if (!goalShown && steps >= GOALS[goalIdx]) { goalShown = true; flashUntil = now + 4000; screenOn = true; lastKey = now; }
    }
  }
}

void readBattery() {
#if HAS_VBAT
  uint32_t mv = (uint32_t)(analogReadMilliVolts(PIN_VBAT) * VBAT_DIV);
  if (mv < 2500) batPct = -1;                      // дільник без живлення: працюємо від USB
  else {
    long p = ((long)mv - 3300) * 100 / (4150 - 3300);   // грубо: 3,30 В = 0 %, 4,15 В = 100 %
    batPct = p < 0 ? 0 : (p > 100 ? 100 : (int)p);
  }
#endif
}

void draw(uint32_t now) {
  bool flash = (int32_t)(flashUntil - now) > 0;
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  char b[32];
  if (flash) {
    oled.invertDisplay((now / 300) % 2 == 0);
    oled.setTextSize(3); oled.setCursor(10, 8); oled.print("GOAL!");
    oled.setTextSize(2); snprintf(b, sizeof b, "%lu", (unsigned long)steps); oled.setCursor(10, 40); oled.print(b);
    oled.display();
    return;
  }
  oled.invertDisplay(false);
  oled.setTextSize(1); oled.setCursor(0, 0); oled.print(running ? "RUN" : "PAUSE");
  if (!adxlOk) { oled.setCursor(40, 0); oled.print("NO ADXL!"); }
  if (batPct >= 0) snprintf(b, sizeof b, "bat %d%%", batPct); else snprintf(b, sizeof b, "USB");
  oled.setCursor(128 - (int)strlen(b) * 6, 0); oled.print(b);
  snprintf(b, sizeof b, "%lu", (unsigned long)steps);
  oled.setTextSize(3); oled.setCursor(0, 12); oled.print(b);
  snprintf(b, sizeof b, "/%lu", (unsigned long)GOALS[goalIdx]);
  oled.setTextSize(1); oled.setCursor(128 - (int)strlen(b) * 6, 30); oled.print(b);
  uint32_t goal = GOALS[goalIdx];
  int fill = steps >= goal ? 124 : (int)((uint64_t)steps * 124 / goal);
  oled.drawRect(0, 40, 128, 6, SSD1306_WHITE);
  oled.fillRect(2, 42, fill, 2, SSD1306_WHITE);
  uint32_t sec = runMs / 1000;
  snprintf(b, sizeof b, "%3lu/min  %02lu:%02lu", (unsigned long)cadence(now), (unsigned long)(sec / 60), (unsigned long)(sec % 60));
  oled.setCursor(0, 56); oled.print(b);
  oled.display();
}

void screenSet(bool on) {
  screenOn = on;
  oled.ssd1306_command(on ? SSD1306_DISPLAYON : SSD1306_DISPLAYOFF);
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
  s1.begin(); s2.begin();
#if HAS_VBAT
  analogSetPinAttenuation(PIN_VBAT, ADC_11db);
#endif
  readBattery();
  lastKey = lastTick = millis();
}

void loop() {
  uint32_t now = millis();
  if (running) runMs += now - lastTick;
  lastTick = now;
  int e1 = s1.poll(now), e2 = s2.poll(now);
  if (e1 || e2) {
    lastKey = now;
    if (!screenOn) { screenSet(true); e1 = e2 = 0; }   // перше натискання лише будить екран
  }
  if (e1 == 1) running = !running;
  if (e1 == 2) { steps = 0; runMs = 0; nT = 0; goalShown = false; flashUntil = 0; }
  if (e2 == 1) { goalIdx = (goalIdx + 1) % NGOALS; goalShown = steps >= GOALS[goalIdx]; }
  if (e2 == 2) screenSet(false);
  if (adxlOk && now - tPoll >= 20) { tPoll = now; pollSteps(now); }
  if (now - tBat >= 10000) { tBat = now; readBattery(); }
  bool flash = (int32_t)(flashUntil - now) > 0;
  if (screenOn && !flash && now - lastKey >= SCREEN_OFF_MS) screenSet(false);
  if (!screenOn && flash) screenSet(true);
  if (oledOk && screenOn && now - tDraw >= 200) { tDraw = now; draw(now); }
}
