#include "display.h"
#include "pins.h"
#include <driver/rtc_io.h>
#include <math.h>
#include <algorithm>
void Display::wake(uint8_t brightness) {
    rtc_gpio_hold_dis(static_cast<gpio_num_t>(PIN_TFT_BL));
    rtc_gpio_deinit(static_cast<gpio_num_t>(PIN_TFT_BL));
    pinMode(PIN_TFT_BL, OUTPUT);
    digitalWrite(PIN_TFT_BL, LOW);
    if (!initialized_) {
        tft_.init();
        tft_.setRotation(1);
        initialized_ = true;
    } else {
        tft_.writecommand(TFT_SLPOUT);
        delay(120);
        tft_.writecommand(TFT_DISPON);
    }
    tft_.fillScreen(TFT_BLACK);
    analogWrite(PIN_TFT_BL, brightness);
    awake_ = true;
}
void Display::sleep() {
    if (initialized_) {
        tft_.writecommand(TFT_DISPOFF);
        tft_.writecommand(TFT_SLPIN);
    }
    analogWrite(PIN_TFT_BL, 0);
    digitalWrite(PIN_TFT_BL, LOW);
    awake_ = false;
}
void Display::header(const char *title) {
    tft_.fillScreen(TFT_BLACK);
    tft_.setTextDatum(TL_DATUM);
    tft_.setTextColor(TFT_CYAN, TFT_BLACK);
    tft_.drawString(title, 6, 3, 2);
    tft_.drawFastHLine(6, 23, 228, TFT_DARKGREY);
    tft_.setTextColor(TFT_WHITE, TFT_BLACK);
}
void Display::values(const Reading &r, float battery, bool external, const Storage &storage) {
    if (!awake_) return;
    header("Environment");
    if (r.valid) {
        char value[32];
        snprintf(value, sizeof(value), "%.1f C", r.temperature);
        tft_.drawString(value, 7, 29, 4);
        snprintf(value, sizeof(value), "%.1f %%", r.humidity);
        tft_.drawString(value, 7, 63, 4);
        snprintf(value, sizeof(value), "%.1f hPa", r.pressure / 100);
        tft_.drawString(value, 137, 70, 2);
    } else {
        tft_.setTextColor(TFT_ORANGE, TFT_BLACK);
        tft_.drawString("BME280 unavailable", 7, 42, 2);
        tft_.drawString(r.error.isEmpty() ? "Check 3V / G / 21 / 22" : r.error, 7, 64, 2);
    }
    tft_.setTextColor(storage.ready() && storage.error().isEmpty() ? TFT_LIGHTGREY : TFT_RED, TFT_BLACK);
    String power = external ? "USB / service" : isfinite(battery) ? String(battery, 2) + " V" : "Battery unknown";
    tft_.drawString(power, 7, 100, 2);
    tft_.drawString(storage.ready() && storage.error().isEmpty() ? "Log OK" : "Log ERROR", 151, 100, 2);
    tft_.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft_.drawString("Right: graph   Left: service info", 7, 120, 1);
}
void Display::graph(Storage &storage, const ClockState &clock) {
    if (!awake_) return;
    header("Last 8 hours");
    constexpr size_t N = 208;
    HistoryBin bins[N];
    storage.history(clock, GRAPH_SECONDS, bins, N);
    for (int channel = 0; channel < 2; ++channel) {
        float low = 10000, high = -10000;
        for (auto &b : bins) if (b.valid && !b.gap) {
            low = std::min(low, (channel ? b.minH : b.minT) / 100.0f);
            high = std::max(high, (channel ? b.maxH : b.maxT) / 100.0f);
        }
        if (high < low) { low = 0; high = 1; }
        if (high - low < 1) { low -= 0.5f; high += 0.5f; }
        const int top = channel ? 86 : 40, height = 25;
        const uint16_t color = channel ? TFT_CYAN : TFT_ORANGE;
        tft_.setTextColor(color, TFT_BLACK);
        char label[48];
        snprintf(label, sizeof(label), "%s %.1f .. %.1f", channel ? "RH %" : "T C", low, high);
        tft_.drawString(label, 7, top - 13, 1);
        tft_.drawFastHLine(16, top + height, N, TFT_DARKGREY);
        int lastX = -1, lastY = 0;
        uint32_t lastSession = 0;
        for (size_t i = 0; i < N; ++i) {
            auto &b = bins[i];
            if (!b.valid || b.gap) { lastX = -1; continue; }
            float min = (channel ? b.minH : b.minT) / 100.0f;
            float max = (channel ? b.maxH : b.maxT) / 100.0f;
            int y1 = top + height - roundf((max - low) * height / (high - low));
            int y2 = top + height - roundf((min - low) * height / (high - low));
            int x = 16 + i, y = (y1 + y2) / 2;
            tft_.drawFastVLine(x, y1, std::max(1, y2 - y1 + 1), color);
            if (lastX >= 0 && lastSession == b.session) tft_.drawLine(lastX, lastY, x, y, color);
            lastX = x; lastY = y; lastSession = b.session;
        }
    }
    tft_.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft_.drawString("-8h", 16, 117, 1);
    tft_.drawString("-4h", 112, 117, 1);
    tft_.drawString("now", 206, 117, 1);
}
void Display::service(const String &network, const String &ip, const String &password) {
    if (!awake_) return;
    header(FORCE_SERVICE_MODE ? "BENCH: Wi-Fi always ON" : "USB service");
    tft_.drawString(network.substring(0, 28), 7, 29, 2);
    tft_.drawString(ip, 7, 51, 2);
    tft_.drawString("User: admin   AP / web password:", 7, 76, 1);
    tft_.drawString(password.length() <= 24 ? password : "Use your saved password", 7, 92, 2);
    tft_.drawString("Right: values", 7, 120, 1);
}
void Display::message(const String &title, const String &text) {
    if (!awake_) return;
    header(title.c_str());
    tft_.drawString(text.substring(0, 29), 7, 46, 2);
}
