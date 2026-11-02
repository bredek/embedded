#include <Arduino.h>
// Wi-Fi метеодатчик: ESP32-S3 DevKitC-1 + DHT11
// DHT11: VCC -> 3V3, GND -> GND, DATA -> GPIO4
// Бібліотеки ставить сам PlatformIO (lib_deps у platformio.ini)

#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>

#include "secrets.h"   // WIFI_NAME і WIFI_PASS (файл не потрапляє в git)

#define DHT_PIN  4
#define DHT_TYPE DHT11

// --- Світлодіод ---
// GPIO6 -> резистор 220 Ом -> LED -> GND. HIGH = світиться.
#define LED_PIN 6

// --- Історія вимірів ---
#define SAMPLE_SEC    300   // як часто записувати точку, секунд (для перевірки постав 10)
#define HISTORY_SIZE  288   // 288 точок * 5 хв = 24 години
#define FIRST_SAMPLE_MS 2000  // перша точка через ~2 с після старту
#define NO_DATA       INT16_MIN  // значення "датчик не відповів"

DHT dht(DHT_PIN, DHT_TYPE);
WebServer server(80);

// Кільцевий буфер: коли місце закінчується, нова точка затирає найстарішу.
// Температура зберігається в десятих градуса (23.4 C -> 234), вологість у %.
int16_t histT[HISTORY_SIZE];
int16_t histH[HISTORY_SIZE];
uint16_t histHead = 0;    // куди писати наступну точку
uint16_t histCount = 0;   // скільки точок вже записано (максимум HISTORY_SIZE)
uint32_t lastSampleMs = 0;  // millis() останнього запису
bool firstSampleDone = false;

bool ledOn = false;   // поточний стан світлодіода (при старті вимкнено)

const char PAGE[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="uk">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Метеостанція</title>
<style>
  :root {
    --bg1: #0f172a; --bg2: #1e293b; --card: rgba(255,255,255,.07);
    --text: #f1f5f9; --muted: #94a3b8; --temp: #fb923c; --hum: #38bdf8; --led: #facc15;
  }
  @media (prefers-color-scheme: light) {
    :root { --bg1: #e0f2fe; --bg2: #f8fafc; --card: rgba(255,255,255,.85);
            --text: #0f172a; --muted: #64748b; --temp: #ea580c; --hum: #0284c7; --led: #eab308; }
  }
  * { box-sizing: border-box; margin: 0; }
  body {
    min-height: 100vh; display: flex; align-items: center; justify-content: center;
    padding: 16px; font-family: -apple-system, "Segoe UI", Roboto, sans-serif;
    color: var(--text); background: linear-gradient(135deg, var(--bg1), var(--bg2));
  }
  main { width: 100%; max-width: 560px; text-align: center; }
  h1 { font-size: 1.4rem; font-weight: 600; margin-bottom: 4px; }
  .sub { color: var(--muted); font-size: .9rem; margin-bottom: 24px; }
  .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(220px, 1fr)); gap: 16px; }
  .card {
    background: var(--card); border-radius: 24px; padding: 28px 16px;
    backdrop-filter: blur(12px); box-shadow: 0 10px 30px rgba(0,0,0,.2);
  }
  .card svg { width: 56px; height: 56px; fill: none; stroke-width: 1.8;
              stroke-linecap: round; stroke-linejoin: round; }
  .temp svg { stroke: var(--temp); color: var(--temp); } .hum svg { stroke: var(--hum); }
  .value { font-size: 3.2rem; font-weight: 700; margin: 10px 0 2px; font-variant-numeric: tabular-nums; }
  .temp .value { color: var(--temp); } .hum .value { color: var(--hum); }
  .label { color: var(--muted); font-size: .95rem; }
  .status { margin-top: 24px; color: var(--muted); font-size: .85rem; }
  .dot { display: inline-block; width: 8px; height: 8px; border-radius: 50%;
         background: #22c55e; margin-right: 6px; }
  .dot.err { background: #ef4444; }
  .light svg { stroke: var(--led); }
  .light .value { font-size: 1.4rem; margin: 6px 0 16px; color: var(--muted); }
  .light.on .value { color: var(--led); }
  /* Великий перемикач: кнопка-доріжка з круглою ручкою */
  .switch { position: relative; width: 96px; height: 52px; border: 0; border-radius: 26px;
            background: rgba(148,163,184,.35); cursor: pointer; transition: background .2s; }
  .switch::after { content: ""; position: absolute; top: 4px; left: 4px; width: 44px; height: 44px;
                   border-radius: 50%; background: #fff; box-shadow: 0 2px 6px rgba(0,0,0,.3);
                   transition: transform .2s; }
  .switch[aria-checked="true"] { background: var(--led); }
  .switch[aria-checked="true"]::after { transform: translateX(44px); }
  .switch:disabled { opacity: .6; cursor: wait; }
  .switch:focus-visible { outline: 3px solid var(--led); outline-offset: 3px; }
  .charts { display: grid; gap: 16px; margin-top: 16px; }
  .chart-card { padding: 20px 16px 16px; }
  .chart-card h2 { font-size: 1rem; font-weight: 600; margin-bottom: 10px; }
  .chart-card canvas { width: 100%; height: 180px; display: block; }
  .chart-card .range { margin-top: 8px; color: var(--muted); font-size: .85rem; }
  .chart-card.temp h2 { color: var(--temp); } .chart-card.hum h2 { color: var(--hum); }
</style>
</head>
<body>
<main>
  <h1>Метеостанція</h1>
  <p class="sub">ESP32-S3 · DHT11</p>
  <div class="grid">
    <section class="card temp">
      <svg viewBox="0 0 23 24"><path d="M14 14.76V3.5a2.5 2.5 0 0 0-5 0v11.26a4.5 4.5 0 1 0 5 0z"/><path d="M11.5 18.5V9"/><circle cx="11.5" cy="18.5" r="1.8" style="fill:currentColor"/></svg>
      <div class="value"><span id="t">--</span>°C</div>
      <div class="label">Температура</div>
    </section>
    <section class="card hum">
      <svg viewBox="0 0 24 24"><path d="M12 3s6 6.4 6 11a6 6 0 0 1-12 0c0-4.6 6-11 6-11z"/><path d="M9.5 14.5a2.5 2.5 0 0 0 2.5 2.5"/></svg>
      <div class="value"><span id="h">--</span>%</div>
      <div class="label">Вологість</div>
    </section>
  </div>
  <section class="card light" id="ledCard" style="margin-top:16px">
    <svg viewBox="0 0 24 24" aria-hidden="true"><path d="M9 18h6M10 21h4M12 3a6 6 0 0 0-3.5 10.9c.6.5 1 1.2 1 2.1h5c0-.9.4-1.6 1-2.1A6 6 0 0 0 12 3z"/></svg>
    <div class="value" id="ledText">--</div>
    <button class="switch" id="ledSw" role="switch" aria-checked="false" aria-label="Світло" disabled></button>
    <div class="label" style="margin-top:12px">Світло</div>
  </section>

  <div class="charts">
    <section class="card chart-card temp">
      <h2>Температура за добу</h2>
      <canvas id="chartT"></canvas>
      <div class="range" id="rangeT">мін — · макс —</div>
    </section>
    <section class="card chart-card hum">
      <h2>Вологість за добу</h2>
      <canvas id="chartH"></canvas>
      <div class="range" id="rangeH">мін — · макс —</div>
    </section>
  </div>
  <p class="status"><span class="dot" id="dot"></span><span id="st">Завантаження…</span></p>
</main>
<script>
  // ---------- Світло ----------
  let ledBusy = false;   // true, поки йде запит до ESP32

  // Показує стан на перемикачі (стан завжди бере з відповіді ESP32)
  function showLed(on) {
    ledSw.setAttribute('aria-checked', on);
    ledSw.disabled = ledBusy;
    ledText.textContent = on ? 'Увімкнено' : 'Вимкнено';
    ledCard.classList.toggle('on', on);
  }

  ledSw.addEventListener('click', async () => {
    const was = ledSw.getAttribute('aria-checked') === 'true';
    ledBusy = true; ledSw.disabled = true;   // блокуємо на час запиту
    try {
      const r = await fetch('/led', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: 'state=' + (was ? 'off' : 'on'),
      });
      if (!r.ok) throw new Error('http ' + r.status);
      const d = await r.json();
      ledBusy = false; showLed(d.on);         // показуємо те, що повернула ESP32
    } catch (e) {
      ledBusy = false; showLed(was);          // помилка: повертаємо старий стан
      dot.className = 'dot err';
      st.textContent = 'Помилка керування світлом';
    }
  });

  async function update() {
    try {
      const r = await fetch('/data');
      const d = await r.json();
      if (d.ok) {
        t.textContent = d.t.toFixed(1);
        h.textContent = Math.round(d.h);
        if (!ledBusy) showLed(d.led);   // синхронізуємо світло з ESP32
        dot.className = 'dot';
        st.textContent = 'Оновлено ' + new Date().toLocaleTimeString('uk-UA');
      } else throw 0;
    } catch (e) {
      dot.className = 'dot err';
      st.textContent = 'Помилка читання датчика';
    }
  }
  update(); setInterval(update, 3000);

  // ---------- Графіки ----------
  let history = null;   // остання відповідь /history
  let fetchedAt = 0;    // коли браузер її отримав

  const pad2 = n => String(n).padStart(2, '0');
  const hhmm = ms => { const d = new Date(ms); return pad2(d.getHours()) + ':' + pad2(d.getMinutes()); };
  const cssVar = name => getComputedStyle(document.documentElement).getPropertyValue(name).trim();

  // Малює один графік. values: масив чисел або null (розрив лінії).
  function drawChart(canvas, values, color, decimals) {
    const dpr = window.devicePixelRatio || 1;
    const w = canvas.clientWidth, hgt = canvas.clientHeight;
    canvas.width = Math.round(w * dpr);
    canvas.height = Math.round(hgt * dpr);
    const c = canvas.getContext('2d');
    c.setTransform(dpr, 0, 0, dpr, 0, 0);   // далі малюємо в CSS-пікселях
    c.clearRect(0, 0, w, hgt);
    c.font = '11px -apple-system, "Segoe UI", Roboto, sans-serif';

    const muted = cssVar('--muted');
    const valid = values.filter(v => v !== null);
    if (values.length < 2 || valid.length < 2) {
      c.fillStyle = muted; c.textAlign = 'center'; c.textBaseline = 'middle';
      c.fillText('Мало даних', w / 2, hgt / 2);
      return;
    }

    // Межі осі Y з невеликим запасом
    let lo = Math.min(...valid), hi = Math.max(...valid);
    if (hi - lo < 1) { lo -= 0.5; hi += 0.5; }
    const margin = (hi - lo) * 0.1;
    lo -= margin; hi += margin;

    const L = 38, R = 8, T = 8, B = 22;               // відступи під підписи
    const pw = w - L - R, ph = hgt - T - B;
    const x = i => L + pw * i / (values.length - 1);
    const y = v => T + ph * (1 - (v - lo) / (hi - lo));

    // Сітка і підписи осі Y (4 лінії)
    c.strokeStyle = muted; c.fillStyle = muted; c.lineWidth = 1;
    c.textAlign = 'right'; c.textBaseline = 'middle';
    for (let k = 0; k < 4; k++) {
      const v = lo + (hi - lo) * k / 3, yy = Math.round(y(v)) + 0.5;
      c.globalAlpha = 0.25;
      c.beginPath(); c.moveTo(L, yy); c.lineTo(w - R, yy); c.stroke();
      c.globalAlpha = 1;
      c.fillText(v.toFixed(decimals), L - 6, yy);
    }

    // Підписи часу по осі X (5 штук)
    c.textBaseline = 'top';
    const n = values.length;
    for (let k = 0; k < 5; k++) {
      const i = Math.round(k * (n - 1) / 4);
      const ms = fetchedAt - (history.age + (n - 1 - i) * history.step) * 1000;
      c.textAlign = k === 0 ? 'left' : k === 4 ? 'right' : 'center';
      c.fillText(hhmm(ms), x(i), hgt - B + 6);
    }

    // Лінія і заливка: ламаємо шлях там, де null
    c.strokeStyle = color; c.fillStyle = color; c.lineWidth = 2;
    c.lineJoin = 'round';
    let start = -1;
    const flush = end => {            // малює відрізок start..end
      if (start < 0) return;
      if (start === end) {            // одинока точка
        c.beginPath(); c.arc(x(start), y(values[start]), 2.5, 0, 7); c.fill();
      } else {
        c.beginPath();
        c.moveTo(x(start), y(values[start]));
        for (let i = start + 1; i <= end; i++) c.lineTo(x(i), y(values[i]));
        c.stroke();
        c.lineTo(x(end), T + ph); c.lineTo(x(start), T + ph); c.closePath();
        c.globalAlpha = 0.18; c.fill(); c.globalAlpha = 1;
      }
      start = -1;
    };
    for (let i = 0; i < n; i++) {
      if (values[i] === null) flush(i - 1);
      else if (start < 0) start = i;
    }
    flush(n - 1);
  }

  function rangeText(values, unit, decimals) {
    const valid = values.filter(v => v !== null);
    if (!valid.length) return 'мін — · макс —';
    return 'мін ' + Math.min(...valid).toFixed(decimals) + ' ' + unit +
           ' · макс ' + Math.max(...valid).toFixed(decimals) + ' ' + unit;
  }

  function redrawCharts() {
    const tv = history ? history.t : [], hv = history ? history.h : [];
    drawChart(document.getElementById('chartT'), tv, cssVar('--temp'), 1);
    drawChart(document.getElementById('chartH'), hv, cssVar('--hum'), 0);
    document.getElementById('rangeT').textContent = rangeText(tv, '°C', 1);
    document.getElementById('rangeH').textContent = rangeText(hv, '%', 0);
  }

  async function loadHistory() {
    try {
      const r = await fetch('/history');
      history = await r.json();
      fetchedAt = Date.now();
      redrawCharts();
    } catch (e) { /* залишаємо стару історію */ }
  }

  loadHistory(); setInterval(loadHistory, 60000);
  window.addEventListener('resize', redrawCharts);
  matchMedia('(prefers-color-scheme: light)').addEventListener('change', redrawCharts);
</script>
</body>
</html>)rawliteral";

// Записує одну точку в кільцевий буфер.
void saveSample() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (isnan(t) || isnan(h)) {
    histT[histHead] = NO_DATA;
    histH[histHead] = NO_DATA;
  } else {
    histT[histHead] = (int16_t)lroundf(t * 10);
    histH[histHead] = (int16_t)lroundf(h);
  }
  histHead = (histHead + 1) % HISTORY_SIZE;
  if (histCount < HISTORY_SIZE) histCount++;
  lastSampleMs = millis();
}

// Викликається з loop(): без delay(), лише порівнюємо millis().
void sampleTick() {
  uint32_t now = millis();
  if (!firstSampleDone) {
    if (now >= FIRST_SAMPLE_MS) {
      firstSampleDone = true;
      saveSample();
    }
  } else if (now - lastSampleMs >= (uint32_t)SAMPLE_SEC * 1000UL) {
    saveSample();
  }
}

void handleRoot() {
  server.send_P(200, "text/html; charset=utf-8", PAGE);
}

void handleData() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  String json;
  if (isnan(t) || isnan(h)) {
    json = "{\"ok\":false,\"led\":" + String(ledOn ? "true" : "false") + "}";
  } else {
    json = "{\"ok\":true,\"t\":" + String(t, 1) + ",\"h\":" + String(h, 0) +
           ",\"led\":" + String(ledOn ? "true" : "false") + "}";
  }
  server.send(200, "application/json", json);
}

// Вмикає/вимикає світлодіод і пише в Serial, хто це зробив (IP клієнта).
void setLed(bool on) {
  ledOn = on;
  digitalWrite(LED_PIN, ledOn ? HIGH : LOW);
  Serial.printf("LED: %s (клієнт %s)\n", ledOn ? "увімкнено" : "вимкнено",
                server.client().remoteIP().toString().c_str());
}

void sendLedState() {
  server.send(200, "application/json", String("{\"on\":") + (ledOn ? "true" : "false") + "}");
}

// GET /led -> {"on":true|false}
void handleLedGet() {
  sendLedState();
}

// POST /led, параметр state = on | off | toggle
void handleLedPost() {
  String state = server.arg("state");
  if (state == "on")           setLed(true);
  else if (state == "off")     setLed(false);
  else if (state == "toggle")  setLed(!ledOn);
  else {
    Serial.printf("LED: неправильний запит state=\"%s\" (клієнт %s)\n", state.c_str(),
                  server.client().remoteIP().toString().c_str());
    server.send(400, "application/json", "{\"error\":\"state must be on, off or toggle\"}");
    return;
  }
  sendLedState();
}

// Історія: від найстаршої точки до найновішої. null = датчик не відповів.
void handleHistory() {
  String json;
  json.reserve(histCount * 14 + 64);
  json = "{\"step\":" + String(SAMPLE_SEC) +
         ",\"age\":" + String(histCount ? (millis() - lastSampleMs) / 1000 : 0);

  // Найстарша точка: якщо буфер повний, це histHead, інакше 0.
  uint16_t first = (histHead + HISTORY_SIZE - histCount) % HISTORY_SIZE;

  json += ",\"t\":[";
  for (uint16_t i = 0; i < histCount; i++) {
    int16_t v = histT[(first + i) % HISTORY_SIZE];
    if (i) json += ',';
    json += (v == NO_DATA) ? String("null") : String(v / 10.0f, 1);
  }
  json += "],\"h\":[";
  for (uint16_t i = 0; i < histCount; i++) {
    int16_t v = histH[(first + i) % HISTORY_SIZE];
    if (i) json += ',';
    json += (v == NO_DATA) ? String("null") : String(v);
  }
  json += "]}";
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  dht.begin();
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);   // при старті світлодіод вимкнено

  WiFi.begin(WIFI_NAME, WIFI_PASS);
  Serial.print("Підключаюсь до Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Готово! Відкрий у браузері: http://");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/history", handleHistory);
  server.on("/led", HTTP_GET, handleLedGet);
  server.on("/led", HTTP_POST, handleLedPost);
  server.begin();
}

void loop() {
  server.handleClient();
  sampleTick();
}
