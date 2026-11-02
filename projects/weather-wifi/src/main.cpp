#include <Arduino.h>
// Wi-Fi метеодатчик: ESP32-S3 DevKitC-1 + DHT11
// DHT11: VCC -> 3V3, GND -> GND, DATA -> GPIO4
// Бібліотеки ставить сам PlatformIO (lib_deps у platformio.ini)

#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>

const char* WIFI_NAME = "TP-Link_3B5D";   // назва домашнього Wi-Fi
const char* WIFI_PASS = "57358680";   // пароль

#define DHT_PIN  4
#define DHT_TYPE DHT11

DHT dht(DHT_PIN, DHT_TYPE);
WebServer server(80);

const char PAGE[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="uk">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Метеостанція</title>
<style>
  :root {
    --bg1: #0f172a; --bg2: #1e293b; --card: rgba(255,255,255,.07);
    --text: #f1f5f9; --muted: #94a3b8; --temp: #fb923c; --hum: #38bdf8;
  }
  @media (prefers-color-scheme: light) {
    :root { --bg1: #e0f2fe; --bg2: #f8fafc; --card: rgba(255,255,255,.85);
            --text: #0f172a; --muted: #64748b; --temp: #ea580c; --hum: #0284c7; }
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
  .temp svg { stroke: var(--temp); } .hum svg { stroke: var(--hum); }
  .value { font-size: 3.2rem; font-weight: 700; margin: 10px 0 2px; font-variant-numeric: tabular-nums; }
  .temp .value { color: var(--temp); } .hum .value { color: var(--hum); }
  .label { color: var(--muted); font-size: .95rem; }
  .status { margin-top: 24px; color: var(--muted); font-size: .85rem; }
  .dot { display: inline-block; width: 8px; height: 8px; border-radius: 50%;
         background: #22c55e; margin-right: 6px; }
  .dot.err { background: #ef4444; }
</style>
</head>
<body>
<main>
  <h1>Метеостанція</h1>
  <p class="sub">ESP32-S3 · DHT11</p>
  <div class="grid">
    <section class="card temp">
      <svg viewBox="0 0 24 24"><path d="M14 14.8V5a2 2 0 0 0-4 0v9.8a4 4 0 1 0 4 0z"/><path d="M12 9v7"/></svg>
      <div class="value"><span id="t">--</span>°C</div>
      <div class="label">Температура</div>
    </section>
    <section class="card hum">
      <svg viewBox="0 0 24 24"><path d="M12 3s6 6.4 6 11a6 6 0 0 1-12 0c0-4.6 6-11 6-11z"/><path d="M9.5 14.5a2.5 2.5 0 0 0 2.5 2.5"/></svg>
      <div class="value"><span id="h">--</span>%</div>
      <div class="label">Вологість</div>
    </section>
  </div>
  <p class="status"><span class="dot" id="dot"></span><span id="st">Завантаження…</span></p>
</main>
<script>
  async function update() {
    try {
      const r = await fetch('/data');
      const d = await r.json();
      if (d.ok) {
        t.textContent = d.t.toFixed(1);
        h.textContent = Math.round(d.h);
        dot.className = 'dot';
        st.textContent = 'Оновлено ' + new Date().toLocaleTimeString('uk-UA');
      } else throw 0;
    } catch (e) {
      dot.className = 'dot err';
      st.textContent = 'Помилка читання датчика';
    }
  }
  update(); setInterval(update, 3000);
</script>
</body>
</html>)rawliteral";

void handleRoot() {
  server.send_P(200, "text/html; charset=utf-8", PAGE);
}

void handleData() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  String json;
  if (isnan(t) || isnan(h)) {
    json = "{\"ok\":false}";
  } else {
    json = "{\"ok\":true,\"t\":" + String(t, 1) + ",\"h\":" + String(h, 0) + "}";
  }
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  dht.begin();

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
  server.begin();
}

void loop() {
  server.handleClient();
}
