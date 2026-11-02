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

1. Set your Wi-Fi name and password in `src/main.cpp` (`WIFI_NAME`, `WIFI_PASS`).
2. Build and upload: `pio run -t upload`.
3. Open the serial monitor (`pio device monitor`, 115200 baud) and find the printed address.
4. Open `http://<device-ip>/` in a browser on the same network.

## Web interface

- `/` — page with temperature and humidity cards (centered, light/dark theme, refreshes every 3 s).
- `/data` — JSON endpoint: `{"ok":true,"t":23.5,"h":45}`; `{"ok":false}` if the sensor cannot be read.

The HTML, CSS and JS are embedded in `src/main.cpp` as a single page.

## Dependencies

Installed automatically by PlatformIO (see `lib_deps` in `platformio.ini`):

- Adafruit DHT sensor library
- Adafruit Unified Sensor
