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
    void values(const Reading &reading, float battery, bool external, const Storage &storage);
    void graph(Storage &storage, const ClockState &clock);
    void service(const String &network, const String &ip, const String &password);
    void message(const String &title, const String &text);
private:
    TFT_eSPI tft_;
    bool initialized_ = false, awake_ = false;
    void header(const char *title);
};
