# Weather WiFi

A PlatformIO project for the Freenove ESP32-S3 WROOM board (N16R8) using the Arduino framework.
It reads temperature and humidity from a DHT11 sensor and serves them on a small web page over Wi-Fi.

## Hardware

| DHT11 | ESP32-S3 |
|-------|----------|
| VCC   | 3V3      |
| GND   | GND      |
| DATA  | GPIO4    |

Light (PWM): GPIO5 -> 1 kΩ -> base of a BC547 transistor. The load (3 LEDs, each with a 220 Ω resistor, from 5 V)
sits in the collector. HIGH = on. PWM is 8-bit at `PWM_FREQ` (5000 Hz by default, e.g. 20000 for a fan).

## Setup

1. Copy `include/secrets.h.example` to `include/secrets.h` and set your Wi-Fi name and password
   (`secrets.h` is in `.gitignore`, so credentials stay out of git).
2. Build and upload: `pio run -t upload`.
3. Open the serial monitor (`pio device monitor`, 115200 baud) and find the printed address.
4. Open `http://<device-ip>/` in a browser on the same network.

## Web interface

- `/` — page with temperature and humidity cards (centered, light/dark theme, refreshes every 3 s)
  and two 24-hour charts drawn on a canvas (no external libraries; history reloads every minute).
  A "Світло" card has a toggle switch and a 0–100 % brightness slider; it shows the state returned by the
  ESP32 and syncs every 3 s, so several phones see the same state. While dragging, requests are sent at most
  every 150 ms, and the final value is always sent on release.
- `/data` — JSON endpoint: `{"ok":true,"t":23.5,"h":45,"on":false,"level":60}`; `{"ok":false,"on":false,"level":60}` if the sensor cannot be read.
- `/pwm` — light control:
  - `GET /pwm` returns `{"on":true,"level":60}`.
  - `POST /pwm` with `on=0|1` and/or `level=0..100` changes the state and returns the new one;
    invalid values return `400`. Every change is logged to Serial with the client IP.
    Example: `curl -X POST -d "on=1&level=60" http://<device-ip>/pwm`.
  - The level (0–100 %) is converted to PWM duty (0–255) with gamma 2.2, so brightness changes look even.
  - `on` and `level` are saved to flash (NVS) at most once per 2 s after a change and restored after reboot.

## History

A point is saved to an in-RAM ring buffer (288 points) shortly after boot and then every `SAMPLE_SEC`
seconds (default 300 = 5 min, so 24 h). Set `SAMPLE_SEC` to 10 in `src/main.cpp` to test quickly.
The history is lost on reboot.

The HTML, CSS and JS are embedded in `src/main.cpp` as a single page.

## Dependencies

Installed automatically by PlatformIO (see `lib_deps` in `platformio.ini`):

- Adafruit DHT sensor library
- Adafruit Unified Sensor
