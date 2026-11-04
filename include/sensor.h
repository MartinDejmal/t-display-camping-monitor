#pragma once
#include <Adafruit_BME280.h>
#include "record.h"
struct Reading {
    bool valid = false;
    float temperature = 0, humidity = 0, pressure = 0;
};
class Sensor {
public:
    void begin();
    Reading measure();
    uint8_t address() const { return address_; }
private:
    Adafruit_BME280 bme_;
    uint8_t address_ = 0;
};
