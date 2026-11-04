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
    header(sensorTypeName(r.type));
    if (r.valid) {
        char value[32];
        snprintf(value, sizeof(value), "%.1f C", r.temperature);
        tft_.drawString(value, 7, 29, 4);
        if (r.humidityAvailable) {
            snprintf(value, sizeof(value), "%.1f %%", r.humidity);
            tft_.drawString(value, 7, 63, 4);
        } else tft_.drawString("RH: n/a", 7, 67, 2);
        snprintf(value, sizeof(value), "%.1f hPa", r.pressure / 100);
        tft_.drawString(value, 137, 70, 2);
    } else {
        tft_.setTextColor(TFT_ORANGE, TFT_BLACK);
        tft_.drawString("Sensor unavailable", 7, 42, 2);
        tft_.drawString(r.error.isEmpty() ? "Check 3V / G / 21 / 22" : r.error, 7, 64, 2);
    }
    tft_.setTextColor(storage.ready() && storage.error().isEmpty() ? TFT_LIGHTGREY : TFT_RED, TFT_BLACK);
    String power = external ? "USB / service" : isfinite(battery) ? String(battery, 2) + " V" : "Battery unknown";
    tft_.drawString(power, 7, 100, 2);
    tft_.drawString(storage.ready() && storage.error().isEmpty() ? "Log OK" : "Log ERROR", 151, 100, 2);
    tft_.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft_.drawString("Right: graph   Left: info", 7, 120, 1);
}
void Display::graph(Storage &storage, const ClockState &clock, bool pressure) {
    if (!awake_) return;
    header("Last 8 hours");
    constexpr size_t N = 208;
    // The Arduino loop stack is only 8 KiB. This single-threaded view can reuse
    // static bins rather than placing ~5 KiB beside filesystem/library frames.
    static HistoryBin bins[N];
    storage.history(clock, GRAPH_SECONDS, bins, N);
    for (int channel = 0; channel < 2; ++channel) {
        float low = 10000, high = -10000;
        for (auto &b : bins) if (b.valid && !b.gap && (!channel || pressure || (b.humidityValid && !b.humidityGap))) {
            low = std::min(low, channel ? (pressure ? b.minP : b.minH) / 100.0f : b.minT / 100.0f);
            high = std::max(high, channel ? (pressure ? b.maxP : b.maxH) / 100.0f : b.maxT / 100.0f);
        }
        bool empty = high < low;
        if (empty) { low = 0; high = 1; }
        if (high - low < 1) { low -= 0.5f; high += 0.5f; }
        const int top = channel ? 86 : 40, height = 25;
        const uint16_t color = channel ? TFT_CYAN : TFT_ORANGE;
        tft_.setTextColor(color, TFT_BLACK);
        char label[48];
        snprintf(label, sizeof(label), "%s %.1f .. %.1f", channel ? (pressure ? "P hPa" : "RH %") : "T C", low, high);
        tft_.drawString(label, 7, top - 13, 1);
        tft_.drawFastHLine(16, top + height, N, TFT_DARKGREY);
        if (empty) tft_.drawString("No data in this session", 20, top + 8, 1);
        int lastX = -1, lastY = 0;
        uint32_t lastSession = 0;
        for (size_t i = 0; i < N; ++i) {
            auto &b = bins[i];
            if (!b.valid || b.gap || (channel && !pressure && (!b.humidityValid || b.humidityGap))) { lastX = -1; continue; }
            float min = channel ? (pressure ? b.minP : b.minH) / 100.0f : b.minT / 100.0f;
            float max = channel ? (pressure ? b.maxP : b.maxH) / 100.0f : b.maxT / 100.0f;
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
void Display::device(const ClockState &clock, Storage &storage, uint32_t interval, bool external) {
    if (!awake_) return;
    header(FORCE_SERVICE_MODE ? "Device: bench / Wi-Fi ON" : external ? "Device: USB" : "Device: battery");
    tft_.drawString("Session " + String(clock.session()) + "  +" + String(clock.elapsedS()) + " s", 7, 30, 2);
    uint64_t now = clock.nowUs(), due = clock.deadline();
    uint32_t remaining = due > now ? (due - now + 999999) / 1000000 : 0;
    tft_.drawString("Next " + String(remaining) + " s / " + String(interval) + " s", 7, 52, 2);
    logger::Record last;
    String log = !storage.ready() ? "History ERROR" : storage.latest(last) ?
        (last.session == clock.session() ? "Last log " + String(clock.elapsedS() - last.elapsedS) + " s ago" :
         "Last log: session " + String(last.session)) : "No records";
    tft_.drawString(log, 7, 74, 2);
    tft_.drawString(clock.epoch() ? "UTC available" : "Time: relative only", 7, 96, 2);
    tft_.drawString("Left: network   Right: values", 7, 120, 1);
}
void Display::service(const String &network, const String &ip, const String &password, bool authRequired) {
    if (!awake_) return;
    header(FORCE_SERVICE_MODE ? "BENCH: Wi-Fi always ON" : "USB service");
    tft_.drawString(network.substring(0, 28), 7, 29, 2);
    tft_.drawString(ip, 7, 51, 2);
    tft_.drawString(authRequired ? "User: admin   AP / web password:" : "Web login OFF | Wi-Fi AP password:", 7, 76, 1);
    tft_.drawString(password.length() <= 24 ? password : "Use your saved password", 7, 92, 2);
    tft_.drawString("Right: values", 7, 120, 1);
}
void Display::message(const String &title, const String &text) {
    if (!awake_) return;
    header(title.c_str());
    tft_.drawString(text.substring(0, 29), 7, 46, 2);
}
