# T-Display Camping Monitor

Firmware for the **TTGO T-Display** (ESP32 + 1.14" ST7789V IPS LCD) as an off-grid battery-powered camping monitor.

## Hardware

| Component | Part |
|-----------|------|
| MCU | ESP32 Xtensa dual-core LX6 |
| Display | 1.14" ST7789V IPS LCD, 135 × 240 px |
| Temp/Humidity | DHT22 (optional, wired to GPIO 27) |
| Internal battery | LiPo via on-board voltage divider → GPIO 34 |
| External battery/solar | Up to ~33 V via user-supplied voltage divider → GPIO 32 |

## Features

- **Three display screens** (cycle with left button):
  - Overview – all readings at a glance
  - Battery – detailed internal LiPo + external battery voltages
  - Environment – temperature and humidity
- **Battery level** shown as percentage with colour-coded icons (green / yellow / red)
- **Power-saving**:
  - Backlight dims after 30 s of inactivity
  - Deep-sleep mode (wake via button or 60 s timer)
- **Button controls**:
  - **Left (GPIO 0)** – cycle screens / wake display
  - **Right (GPIO 35)** – toggle backlight / wake display

## Dev Stack

- [PlatformIO](https://platformio.org/) (`platformio.ini` included)
- Framework: Arduino (ESP32)
- Libraries:
  - [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) – display driver
  - [DHT sensor library](https://github.com/adafruit/DHT-sensor-library) – temperature/humidity

## Getting Started

### 1. Wiring

| Signal | GPIO | Notes |
|--------|------|-------|
| DHT22 data | 27 | Add 10 kΩ pull-up to 3.3 V |
| External battery (+) | 32 | Via voltage divider (see below) |

**External voltage divider for 12 V battery monitoring:**

```
Battery (+) ──┬── R1 (1 MΩ) ──── GPIO 32
              └── R2 (100 kΩ) ── GND
```

This gives a ÷11 ratio (up to ~33 V input → 3.0 V at ADC).  
Adjust `VEXT_DIVIDER_RATIO` in `include/config.h` if you use different resistor values.

### 2. Build & Flash

```bash
# Install PlatformIO CLI or open the project in VS Code with the PlatformIO extension
pio run -e ttgo-t-display          # build
pio run -e ttgo-t-display -t upload # flash
pio device monitor                  # serial monitor (115200 baud)
```

### 3. Configuration

All tuneable parameters are in `include/config.h`:

| Constant | Default | Description |
|----------|---------|-------------|
| `BACKLIGHT_DEFAULT` | 200 | Startup brightness (0-255) |
| `BACKLIGHT_TIMEOUT_MS` | 30 000 ms | Time before auto-dim |
| `SLEEP_INTERVAL_US` | 60 000 000 µs (60 s) | Deep-sleep wakeup interval |
| `SCREEN_CYCLE_MS` | 10 000 ms | Auto-advance screen period |
| `VEXT_DIVIDER_RATIO` | 11.0 | External ADC voltage-divider ratio |

