#pragma once
#include <Adafruit_BME280.h>
#include "record.h"
struct Reading {
    bool valid = false;
    float temperature = 0, humidity = 0, pressure = 0;
    uint8_t address = 0, chipId = 0;
    String error;
};
class Sensor {
public:
    void begin();
    Reading measure();
    void diagnose();
    uint8_t address() const { return address_; }
private:
    Adafruit_BME280 bme_;
    uint8_t address_ = 0;
    uint8_t detectedAddress_ = 0, chipId_ = 0;
    bool lineFault_ = false;
    String error_;
};
