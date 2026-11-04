#pragma once
#include <Arduino.h>
class Power {
public:
    void begin();
    bool poll(); // returns true on stable external supply transition
    bool external() const { return external_; }
    float batteryVoltage(); // NaN while on USB; divider measures a shared rail
private:
    bool external_ = false, candidate_ = false;
    uint32_t changed_ = 0, polled_ = 0;
    bool sense();
};
