#pragma once
#include <Arduino.h>
#include "pins.h"
#ifndef USB_SENSE_WIRED
#define USB_SENSE_WIRED 0
#endif
#ifndef FORCE_SERVICE_MODE
#define FORCE_SERVICE_MODE 0
#endif
#ifndef FW_VERSION
#define FW_VERSION "dev"
#endif
#define LOGGER_STRINGIFY_INNER(x) #x
#define LOGGER_STRINGIFY(x) LOGGER_STRINGIFY_INNER(x)
#if USB_SENSE_WIRED
#define LOGGER_SENSE_MARKER ":SENSE=" LOGGER_STRINGIFY(USB_SENSE_GPIO)
#else
#define LOGGER_SENSE_MARKER ""
#endif
// Compatibility marker, not a cryptographic signature. Used by web OTA preflight.
constexpr char FIRMWARE_FAMILY[] = "|TDLOGGER:ORIGINAL:USB=" LOGGER_STRINGIFY(USB_SENSE_WIRED)
    ":BENCH=" LOGGER_STRINGIFY(FORCE_SERVICE_MODE) ":RECORD=2:PART=1:CONFIG=2" LOGGER_SENSE_MARKER "|";
constexpr uint32_t DEFAULT_INTERVAL_S = 60;
constexpr uint32_t HISTORY_SECONDS = 30UL * 86400;
constexpr uint32_t GRAPH_SECONDS = 8UL * 3600;
constexpr uint32_t MIN_INTERVAL_S = 60;
constexpr uint32_t MAX_INTERVAL_S = 3600;
constexpr uint32_t MIN_VALID_EPOCH = 1704067200UL;
constexpr uint32_t MAX_VALID_EPOCH = 2145916800UL; // this Arduino/IDF uses signed 32-bit time_t
constexpr uint32_t LOW_BATTERY_RECHECK_S = 900;
constexpr uint32_t BLOCK_RECORDS = 1440;
constexpr float LOW_BATTERY_V = 3.40f; // calibrate on actual board
constexpr float OTA_MIN_BATTERY_V = 3.65f;
constexpr size_t WIFI_PROFILE_COUNT = 5;
struct WifiProfile {
    String ssid;
    String password;
    bool hidden = false;
};
struct Settings {
    uint32_t intervalS = DEFAULT_INTERVAL_S;
    uint16_t screenTimeoutS = 15;
    uint8_t brightness = 120;
    bool authEnabled = true;
    bool autoTime = true;
    WifiProfile wifi[WIFI_PROFILE_COUNT];
    String adminPassword;
};
