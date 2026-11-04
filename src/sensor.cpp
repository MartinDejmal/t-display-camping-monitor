#include "sensor.h"
#include "pins.h"
#include <Wire.h>
#include <math.h>
void Sensor::begin() {
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    Wire.setTimeOut(50);
    address_ = 0;
    for (uint8_t addr : {0x76, 0x77}) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0 && bme_.begin(addr, &Wire)) {
            address_ = addr;
            break;
        }
    }
    if (address_) bme_.setSampling(Adafruit_BME280::MODE_FORCED,
        Adafruit_BME280::SAMPLING_X1, Adafruit_BME280::SAMPLING_X1,
        Adafruit_BME280::SAMPLING_X1, Adafruit_BME280::FILTER_OFF);
}
Reading Sensor::measure() {
    if (!address_) begin();
    Reading r;
    if (!address_ || !bme_.takeForcedMeasurement()) return r;
    // x1 T/P/RH conversion takes < 12 ms; also covers the status-bit start delay.
    delay(12);
    r.temperature = bme_.readTemperature();
    r.humidity = bme_.readHumidity();
    r.pressure = bme_.readPressure();
    r.valid = isfinite(r.temperature) && isfinite(r.humidity) && isfinite(r.pressure) &&
        r.temperature >= -40 && r.temperature <= 85 &&
        r.humidity >= 0 && r.humidity <= 100 && r.pressure >= 30000 && r.pressure <= 110000;
    if (!r.valid) address_ = 0;
    return r;
}
