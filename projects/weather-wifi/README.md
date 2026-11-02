# Weather WiFi

A PlatformIO project for the Freenove ESP32-S3 WROOM board (N16R8) using the Arduino framework.
It reads temperature and humidity from a DHT11 sensor and serves them on a small web page over Wi-Fi.

## Hardware

| DHT11 | ESP32-S3 |
|-------|----------|
| VCC   | 3V3      |
| GND   | GND      |
| DATA  | GPIO4    |

## Setup

1. Copy `include/secrets.h.example` to `include/secrets.h` and set your Wi-Fi name and password
   (`secrets.h` is in `.gitignore`, so credentials stay out of git).
2. Build and upload: `pio run -t upload`.
3. Open the serial monitor (`pio device monitor`, 115200 baud) and find the printed address.
4. Open `http://<device-ip>/` in a browser on the same network.

## Web interface

- `/` — page with temperature and humidity cards (centered, light/dark theme, refreshes every 3 s)
  and two 24-hour charts drawn on a canvas (no external libraries; history reloads every minute).
- `/data` — JSON endpoint: `{"ok":true,"t":23.5,"h":45}`; `{"ok":false}` if the sensor cannot be read.
- `/history` — JSON with the last 24 h, oldest point first:
  `{"step":300,"age":120,"t":[23.4,null,...],"h":[45,null,...]}`.
  `step` is seconds between points, `age` is seconds since the newest point, `null` means the sensor did not respond.
  The browser computes each point's time as `now - (age + (n-1-i)*step)`, so no NTP is needed.

## History

A point is saved to an in-RAM ring buffer (288 points) shortly after boot and then every `SAMPLE_SEC`
seconds (default 300 = 5 min, so 24 h). Set `SAMPLE_SEC` to 10 in `src/main.cpp` to test quickly.
The history is lost on reboot.

The HTML, CSS and JS are embedded in `src/main.cpp` as a single page.

## Dependencies

Installed automatically by PlatformIO (see `lib_deps` in `platformio.ini`):

- Adafruit DHT sensor library
- Adafruit Unified Sensor
