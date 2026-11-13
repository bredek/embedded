# advanced-c-blink

Lab 11: Embedded C++ blink on an ESP32-S3 DevKitC-1 (N16R8) using the Arduino framework and PlatformIO. Everything lives in a single file, `src/main.cpp`.

## Wiring

| Part   | Connection                                                  |
|--------|-------------------------------------------------------------|
| LED    | GPIO21 -> 220 Ω -> LED anode (long leg), cathode -> GND     |
| Button | GPIO16 -> button -> GND (internal pull-up, pressed = LOW)   |

## Behavior

A button press cycles through three modes:

1. **BLINK**: the LED toggles every 500 ms (non-blocking, based on `millis()`, no `delay()`).
2. **ALWAYS_ON**: the LED is permanently on.
3. **ALWAYS_OFF**: the LED is permanently off.

The device starts in BLINK mode. Each mode change is printed to the serial monitor (115200 baud).

## Code structure

- `Config`: all parameters as `static constexpr` (pins, intervals, debounce, serial settings), with `static_assert` sanity checks.
- `Led` and `Blinker`: LED control with `LedState` enum and a non-blocking blink.
- `Button`: interrupt-driven input. The ISR only sets a `volatile bool` flag; debouncing is done in `loop()` with a "quiet for N ms" algorithm (30 ms by default).
- `LoopStats`: measures `loop()` execution time in CPU cycles and prints avg/max/period every 1000 iterations (disable via `Config::kLoopStatsEnabled`).
- `App`: ties everything together; `setup()` and `loop()` just call `app().init()` and `app().tick()`.

## Build and run

```
pio run -t upload -t monitor
```

If the serial monitor stays silent when connected via the "USB" port (not "UART"/"COM"), add `-D ARDUINO_USB_CDC_ON_BOOT=1` to `build_flags` in `platformio.ini`.
