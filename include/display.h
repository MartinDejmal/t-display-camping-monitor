#pragma once
#include <TFT_eSPI.h>
#include "config.h"
#include "sensor.h"
#include "storage.h"
class Display {
public:
    void wake(uint8_t brightness);
    void sleep();
    bool awake() const { return awake_; }
    void values(const Reading &reading, float battery, bool external, Storage &storage, uint32_t session);
    void graph(Storage &storage, const ClockState &clock, bool pressure);
    void device(const ClockState &clock, Storage &storage, uint32_t interval, bool external);
    void service(const String &network, const String &ip, const String &password, bool authRequired);
    void message(const String &title, const String &text);
private:
    TFT_eSPI tft_;
    bool initialized_ = false, awake_ = false;
    void header(const char *title);
    void unitText(const char *text, int x, int y, uint8_t font, uint16_t color);
};
