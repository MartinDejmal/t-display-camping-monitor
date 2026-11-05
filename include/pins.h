#pragma once
// Original ESP32 T-Display. Silkscreen labels: 3V, G, 21, 22, 33.
constexpr int PIN_TFT_BL = 4;
constexpr int PIN_BTN_RIGHT = 35; // external board pull-up, active LOW
constexpr int PIN_BTN_LEFT = 0;   // boot strap; used only after boot
constexpr int PIN_I2C_SDA = 21;
constexpr int PIN_I2C_SCL = 22;
constexpr int PIN_BAT_ADC = 34;
constexpr int PIN_ADC_ENABLE = 14;
#define USB_SENSE_GPIO 33
constexpr int PIN_USB_SENSE = USB_SENSE_GPIO; // actual USB VBUS -> 100k -> 33 -> 100k -> G
