#pragma once

// ============================================================
// TTGO T-Display pin definitions
// Board: ESP32 + 1.14" ST7789V IPS LCD (135x240)
// ============================================================

// --- TFT display (configured via build_flags for TFT_eSPI) ---
#define PIN_TFT_MOSI 19
#define PIN_TFT_SCLK 18
#define PIN_TFT_CS   5
#define PIN_TFT_DC   16
#define PIN_TFT_RST  23
#define PIN_TFT_BL   4   // Backlight (active HIGH)

// --- On-board buttons ---
#define PIN_BTN_LEFT  0   // GPIO0  – active LOW, boot button
#define PIN_BTN_RIGHT 35  // GPIO35 – active LOW

// --- ADC: internal LiPo battery voltage divider (1:2) ---
// Actual voltage = ADC_reading * (3.3 / 4095) * 2
#define PIN_BAT_ADC   34  // Read-only GPIO

// --- External battery / solar (user-wired voltage divider) ---
// Connect via a resistor divider so the voltage stays below 3.3 V.
// Default divider ratio R2/(R1+R2) = 100k/1100k ≈ 1:11 (for 0-33 V range).
#define PIN_EXT_ADC   32

// --- DHT22 temperature/humidity sensor ---
#define PIN_DHT       27
