// Лаба 11: Embedded C++ blink, усе в одному файлі.
//   - LED: enum class LedState, клас Led з init()/set(), blink без delay() на millis()
//   - усі параметри в struct Config (static constexpr), без магічних чисел
//   - кнопка на перериванні: ISR лише ставить volatile bool прапорець
//   - режими: Блимання -> Постійно увімкнено -> Постійно вимкнено -> Блимання
//   - антидребезг і перемикання режимів у loop(), замір часу loop() кожні 1000 ітерацій
// Плата: ESP32-S3 DevKitC-1 (N16R8).
//   LED:    GPIO21 -> 220 Ом -> LED (довга ніжка) -> LED (коротка) -> GND
//   Кнопка: GPIO16 -> кнопка -> GND, внутрішній pull-up (як у лабі 10)

#include <Arduino.h>
#include <stdarg.h>
#include <stdio.h>

// ================================================================ конфігурація
struct Config {
  Config() = delete;

  // --- залізо
  static constexpr uint8_t kLedPin        = 21;
  static constexpr bool    kLedActiveHigh = true;
  static constexpr uint8_t kButtonPin     = 16;   // той самий пін, що в лабі 10 (брязкіт)

  // --- час
  static constexpr uint32_t kBlinkIntervalMs = 500;
  // Скільки мс після ОСТАННЬОГО фронту кнопка має бути «тихою», щоб ми їй повірили.
  // Постав сюди свій мінімальний debounce з лаби 10, помножений на 2-3 (запас).
  static constexpr uint32_t kDebounceMs = 30;

  // --- Serial і статистика
  static constexpr uint32_t kSerialBaud       = 115200;
  static constexpr bool     kLoopStatsEnabled = true;    // false = не друкувати час loop()
  static constexpr uint32_t kReportEvery      = 1000;
  static constexpr size_t   kLineBufSize      = 128;
  static constexpr uint32_t kNsPerUs          = 1000;
};

static_assert(Config::kLedPin != Config::kButtonPin, "LED і кнопка на одному піні");
static_assert(Config::kDebounceMs > 0 && Config::kDebounceMs < 200, "debounce 1..199 мс");
static_assert(Config::kBlinkIntervalMs > Config::kDebounceMs, "інтервал блимання має бути довшим за debounce");

// ================================================================ ISR і її прапорець
// Це ЄДИНА змінна на рівні файлу. volatile: її змінює переривання, тому компілятор
// мусить щоразу читати її з пам'яті, а не тримати копію в регістрі.
// static: ім'я видно лише в цьому .cpp (внутрішнє зв'язування).
static volatile bool g_buttonEdge = false;

// IRAM_ATTR: код ISR лежить у швидкій внутрішній RAM, а не у flash.
// Тут НЕМАЄ Serial, delay, millis-логіки, malloc. Лише «запам'ятати» і вийти.
static void IRAM_ATTR onButtonEdge() {
  g_buttonEdge = true;
}

// Забрати прапорець у loop(). Якщо ISR спрацює між читанням і скиданням, ми «загубимо»
// лише ще один фронт того самого брязкоту: це нешкідливо, бо далі все одно чекаємо тиші.
static bool takeButtonEdge() {
  if (!g_buttonEdge) return false;
  g_buttonEdge = false;
  return true;
}

// ================================================================ LED
enum class LedState : uint8_t { Off, On };

class Led {
 public:
  constexpr Led(uint8_t pin, bool activeHigh) : pin_(pin), activeHigh_(activeHigh), state_(LedState::Off) {}
  void init() {
    pinMode(pin_, OUTPUT);
    set(LedState::Off);
  }
  void set(LedState state) {
    state_ = state;
    const bool levelHigh = (state == LedState::On) == activeHigh_;
    digitalWrite(pin_, levelHigh ? HIGH : LOW);
  }
  void toggle() { set(state_ == LedState::On ? LedState::Off : LedState::On); }
  LedState state() const { return state_; }

 private:
  const uint8_t pin_;
  const bool activeHigh_;
  LedState state_;
};

class Blinker {
 public:
  Blinker(Led& led, uint32_t intervalMs) : led_(led), intervalMs_(intervalMs) {}
  void restart(uint32_t nowMs) { lastToggleMs_ = nowMs; }
  void update(uint32_t nowMs) {
    if (nowMs - lastToggleMs_ >= intervalMs_) {
      lastToggleMs_ += intervalMs_;
      led_.toggle();
    }
  }

 private:
  Led& led_;
  const uint32_t intervalMs_;
  uint32_t lastToggleMs_ = 0;
};

// ================================================================ режими
enum class Mode : uint8_t { Blink, AlwaysOn, AlwaysOff };

static Mode nextMode(Mode m) {
  switch (m) {
    case Mode::Blink:     return Mode::AlwaysOn;
    case Mode::AlwaysOn:  return Mode::AlwaysOff;
    case Mode::AlwaysOff: return Mode::Blink;
  }
  return Mode::Blink;   // сюди не дійдемо, але компілятор любить повний return
}

static const char* modeName(Mode m) {
  switch (m) {
    case Mode::Blink:     return "BLINK";
    case Mode::AlwaysOn:  return "ALWAYS_ON";
    case Mode::AlwaysOff: return "ALWAYS_OFF";
  }
  return "?";
}

// ================================================================ кнопка з антидребезгом
// Алгоритм «тиша N мс»: кожен фронт з ISR перезапускає таймер. Рівень піна читаємо,
// лише коли N мс поспіль не було жодного фронту. Нове натискання = стабільний LOW,
// якщо до того був стабільний HIGH.
class Button {
 public:
  constexpr Button(uint8_t pin, uint32_t debounceMs) : pin_(pin), debounceMs_(debounceMs) {}

  void init(void (*isr)()) {
    pinMode(pin_, INPUT_PULLUP);           // відпущена = HIGH, натиснута = LOW
    stablePressed_ = readPressed();        // якщо тримали кнопку під час старту, це не натискання
    attachInterrupt(digitalPinToInterrupt(pin_), isr, FALLING);
  }

  // Викликати в кожній ітерації loop(). Повертає true рівно один раз на натискання.
  bool update(uint32_t nowMs, bool edgeSeen) {
    if (edgeSeen) lastEdgeMs_ = nowMs;                    // ще брязкотить: таймер з нуля
    if (nowMs - lastEdgeMs_ < debounceMs_) return false;  // тиша ще не настала
    const bool pressed = readPressed();
    if (pressed == stablePressed_) return false;          // нічого нового
    stablePressed_ = pressed;
    return pressed;                                       // true лише на натисканні, не на відпусканні
  }

 private:
  bool readPressed() const { return digitalRead(pin_) == LOW; }

  const uint8_t pin_;
  const uint32_t debounceMs_;
  uint32_t lastEdgeMs_ = 0;
  bool stablePressed_ = false;
};

// ================================================================ статистика часу loop()
class LoopStats {
 public:
  void reset(uint32_t nowUs) {
    count_ = 0; sumCycles_ = 0; minCycles_ = UINT32_MAX; maxCycles_ = 0; windowStartUs_ = nowUs;
  }
  void add(uint32_t cycles) {
    ++count_; sumCycles_ += cycles;
    if (cycles < minCycles_) minCycles_ = cycles;
    if (cycles > maxCycles_) maxCycles_ = cycles;
  }
  bool full() const { return count_ >= Config::kReportEvery; }
  uint32_t avgBodyNs(uint32_t mhz) const { return static_cast<uint32_t>(sumCycles_ * Config::kNsPerUs / count_ / mhz); }
  uint32_t maxBodyNs(uint32_t mhz) const { return static_cast<uint32_t>(uint64_t(maxCycles_) * Config::kNsPerUs / mhz); }
  uint32_t avgPeriodNs(uint32_t nowUs) const { return static_cast<uint32_t>(uint64_t(nowUs - windowStartUs_) * Config::kNsPerUs / count_); }

 private:
  uint32_t count_ = 0;
  uint64_t sumCycles_ = 0;
  uint32_t minCycles_ = UINT32_MAX;
  uint32_t maxCycles_ = 0;
  uint32_t windowStartUs_ = 0;
};

// ================================================================ застосунок
class App {
 public:
  void init() {
    Serial.begin(Config::kSerialBaud);
    led_.init();
    button_.init(onButtonEdge);
    cpuMhz_ = getCpuFrequencyMhz();
    Serial.println();
    Serial.println("lab-11: Embedded C++ blink + button");
    printLine("LED GPIO%u, button GPIO%u, debounce %lu ms, free heap %lu B", unsigned(Config::kLedPin),
              unsigned(Config::kButtonPin), (unsigned long)Config::kDebounceMs, (unsigned long)ESP.getFreeHeap());
    enterMode(Mode::Blink, millis());
    stats_.reset(micros());
  }

  void tick() {
    const uint32_t c0 = ESP.getCycleCount();
    const uint32_t nowMs = millis();

    if (button_.update(nowMs, takeButtonEdge())) {
      ++presses_;
      enterMode(nextMode(mode_), nowMs);
    }
    if (mode_ == Mode::Blink) blinker_.update(nowMs);

    if (Config::kLoopStatsEnabled) {   // константа: якщо false, компілятор викине цей блок
      stats_.add(ESP.getCycleCount() - c0);
      if (stats_.full()) {
        const uint32_t nowUs = micros();
        printLine("loop body avg %lu max %lu ns | period %lu ns | heap %lu", (unsigned long)stats_.avgBodyNs(cpuMhz_),
                  (unsigned long)stats_.maxBodyNs(cpuMhz_), (unsigned long)stats_.avgPeriodNs(nowUs),
                  (unsigned long)ESP.getFreeHeap());
        stats_.reset(micros());
      }
    }
  }

 private:
  void enterMode(Mode m, uint32_t nowMs) {
    mode_ = m;
    switch (m) {
      case Mode::Blink:
        led_.set(LedState::On);   // одразу видно, що режим змінився
        blinker_.restart(nowMs);
        break;
      case Mode::AlwaysOn:  led_.set(LedState::On);  break;
      case Mode::AlwaysOff: led_.set(LedState::Off); break;
    }
    printLine("press #%lu -> mode %s (t=%lu ms)", (unsigned long)presses_, modeName(m), (unsigned long)nowMs);
  }

  // format(printf, 2, 3): компілятор перевіряє %lu/%u проти аргументів, як у звичайного printf.
  __attribute__((format(printf, 2, 3))) void printLine(const char* fmt, ...) {
    char buf[Config::kLineBufSize];   // на стеку, жодного malloc
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    Serial.println(buf);
  }

  Led led_{Config::kLedPin, Config::kLedActiveHigh};
  Blinker blinker_{led_, Config::kBlinkIntervalMs};
  Button button_{Config::kButtonPin, Config::kDebounceMs};
  LoopStats stats_;
  Mode mode_ = Mode::Blink;
  uint32_t presses_ = 0;
  uint32_t cpuMhz_ = 0;
};

static App& app() {
  static App app_;
  return app_;
}

void setup() { app().init(); }
void loop() { app().tick(); }
