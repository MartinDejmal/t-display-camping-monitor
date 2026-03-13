#pragma once

// ============================================================
// Camping monitor – runtime configuration
// ============================================================

// --- Display ---
#define TFT_ROTATION        1       // 0=portrait, 1=landscape, 2=portrait-flip, 3=landscape-flip
#define BACKLIGHT_DEFAULT  200      // PWM duty cycle 0-255 (display brightness at startup)
#define BACKLIGHT_DIM       30      // Dimmed brightness level

// --- Screen auto-cycle & backlight timeout (milliseconds) ---
#define SCREEN_CYCLE_MS    10000UL  // Advance screen every 10 s when idle
#define BACKLIGHT_TIMEOUT_MS 30000UL // Dim backlight after 30 s without button press

// --- Deep-sleep interval (microseconds) ---
// The device wakes every SLEEP_INTERVAL_US, refreshes readings and goes back to sleep.
// Waking via button press overrides the timer.
#define SLEEP_INTERVAL_US  (60ULL * 1000000ULL)  // 60 seconds

// --- Sampling ---
#define ADC_SAMPLES         16      // Number of ADC readings to average
#define DHT_TYPE            DHT22

// --- Internal LiPo ADC scaling ---
// On-board voltage divider is 1:2; ADC reference is 3.3 V, resolution 12-bit (4096 steps).
// The *2.0f factor compensates for the 1:2 hardware divider that halves the battery voltage.
// Formula: V_bat = raw * (3.3f / 4095.0f) * 2.0f
#define BAT_ADC_SCALE       (3.3f / 4095.0f * 2.0f)

// --- External battery/solar ADC scaling ---
// Adjust VEXT_DIVIDER_RATIO to match your actual voltage-divider wiring.
// Example: R1=1 MOhm, R2=100 kOhm → ratio = (R1+R2)/R2 = 11.0
#define VEXT_DIVIDER_RATIO  11.0f
#define VEXT_ADC_SCALE      (3.3f / 4095.0f * VEXT_DIVIDER_RATIO)

// --- Battery thresholds (volts) – for LiPo percentage estimation ---
#define LIPO_VOLT_MAX       4.2f
#define LIPO_VOLT_MIN       3.0f

// --- External battery thresholds (volts) – 12 V lead-acid / LiFePO4 ---
#define EXT_VOLT_MAX        14.4f   // Fully charged (lead-acid bulk charge)
#define EXT_VOLT_MIN        11.5f   // Low-battery cutoff

// --- Screen IDs ---
#define SCREEN_OVERVIEW     0
#define SCREEN_BATTERY      1
#define SCREEN_ENVIRONMENT  2
#define SCREEN_COUNT        3
