#include "sensor.h"
#include "pins.h"
#include <Wire.h>
#include <math.h>
#include <driver/gpio.h>
namespace {
bool readRegister(uint8_t address, uint8_t reg, uint8_t &value) {
    Wire.beginTransmission(address);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom(address, static_cast<uint8_t>(1)) != 1) return false;
    value = Wire.read();
    return true;
}
bool checkLines(int sdaPin, int sclPin, String &error) {
    const auto sda = static_cast<gpio_num_t>(sdaPin), scl = static_cast<gpio_num_t>(sclPin);
    for (auto pin : {sda, scl}) {
        gpio_hold_dis(pin);
        gpio_reset_pin(pin);
        gpio_set_direction(pin, GPIO_MODE_INPUT_OUTPUT_OD);
        gpio_set_pull_mode(pin, GPIO_PULLUP_ONLY);
        gpio_set_level(pin, 1);
    }
    delay(2);
    auto levels = [&](const char *label) {
        Serial.printf("[GPIO] SDA=%d SCL=%d %s: %d/%d\n", sdaPin, sclPin, label,
            gpio_get_level(sda), gpio_get_level(scl));
    };
    levels("released");
    if (!gpio_get_level(sda) || !gpio_get_level(scl)) {
        error = !gpio_get_level(scl) ? "SCL held LOW" : "SDA held LOW";
        gpio_set_direction(sda, GPIO_MODE_INPUT);
        gpio_set_direction(scl, GPIO_MODE_INPUT);
        Serial.printf("[I2C] %s; communication stopped\n", error.c_str());
        return false;
    }
    gpio_set_level(sda, 0); delayMicroseconds(50); levels("SDA driven LOW");
    bool sdaLow = gpio_get_level(sda) == 0;
    gpio_set_level(sda, 1);
    gpio_set_level(scl, 0); delayMicroseconds(50); levels("SCL driven LOW");
    bool sclLow = gpio_get_level(scl) == 0;
    gpio_set_level(scl, 1);
    if (!sdaLow || !sclLow) {
        error = !sdaLow ? "SDA pin stays HIGH" : "SCL pin stays HIGH";
        gpio_set_direction(sda, GPIO_MODE_INPUT);
        gpio_set_direction(scl, GPIO_MODE_INPUT);
        Serial.printf("[I2C] %s; communication stopped until reset or explicit 'i' diagnostic\n", error.c_str());
        return false;
    }
    return true;
}
bool softwareProbe(int sdaPin, int sclPin, String &error) {
    if (!checkLines(sdaPin, sclPin, error)) return false;
    const auto sda = static_cast<gpio_num_t>(sdaPin), scl = static_cast<gpio_num_t>(sclPin);
    auto clockHigh = [&]() {
        gpio_set_level(scl, 1);
        uint32_t start = micros();
        while (!gpio_get_level(scl) && micros() - start < 2000) delayMicroseconds(5);
        delayMicroseconds(20);
        return gpio_get_level(scl) != 0;
    };
    auto start = [&]() {
        gpio_set_level(sda, 1); clockHigh();
        gpio_set_level(sda, 0); delayMicroseconds(20); gpio_set_level(scl, 0);
    };
    auto stop = [&]() {
        gpio_set_level(sda, 0); delayMicroseconds(20); clockHigh();
        gpio_set_level(sda, 1); delayMicroseconds(20);
    };
    auto write = [&](uint8_t value) {
        for (int bit = 7; bit >= 0; --bit) {
            gpio_set_level(sda, (value >> bit) & 1); delayMicroseconds(20);
            if (!clockHigh()) { gpio_set_level(scl, 0); return false; }
            gpio_set_level(scl, 0);
        }
        gpio_set_level(sda, 1); delayMicroseconds(20);
        bool raised = clockHigh();
        bool ack = raised && !gpio_get_level(sda);
        gpio_set_level(scl, 0); delayMicroseconds(20);
        return ack;
    };
    // Nine clocks + STOP also release a slave interrupted in a previous read.
    for (int i = 0; i < 9; ++i) { gpio_set_level(scl, 0); delayMicroseconds(20); clockHigh(); }
    stop();
    for (uint8_t address : {0x76, 0x77}) {
        start();
        bool ack = write(address << 1);
        bool reg = ack && write(0xd0);
        uint8_t id = 0;
        bool read = false;
        if (reg) {
            start();
            read = write((address << 1) | 1);
            if (read) {
                gpio_set_level(sda, 1);
                for (int i = 0; i < 8; ++i) {
                    delayMicroseconds(20); clockHigh();
                    id = (id << 1) | gpio_get_level(sda); gpio_set_level(scl, 0);
                }
                gpio_set_level(sda, 1); clockHigh(); gpio_set_level(scl, 0); // NACK
            }
        }
        stop();
        Serial.printf("[SOFT-I2C] SDA=%d SCL=%d address=0x%02X ACK=%s chipID_read=%s ID=0x%02X\n",
            sdaPin, sclPin, address, ack ? "yes" : "no", read ? "ok" : "failed", id);
    }
    gpio_set_direction(sda, GPIO_MODE_INPUT);
    gpio_set_direction(scl, GPIO_MODE_INPUT);
    return true;
}
}
void Sensor::begin() {
    if (lineFault_) return;
    Wire.end();
    address_ = 0;
    detectedAddress_ = chipId_ = 0;
    if (!checkLines(PIN_I2C_SDA, PIN_I2C_SCL, error_)) { lineFault_ = true; return; }
    if (!Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 100000)) { error_ = "I2C driver start failed"; return; }
    Wire.setTimeOut(50);
    error_ = "No I2C response";
    Serial.printf("[BME] probe SDA=%d SCL=%d levels=%d/%d\n", PIN_I2C_SDA, PIN_I2C_SCL,
        digitalRead(PIN_I2C_SDA), digitalRead(PIN_I2C_SCL));
    for (uint8_t addr : {0x76, 0x77}) {
        Wire.beginTransmission(addr);
        uint8_t code = Wire.endTransmission();
        if (code != 0) { Serial.printf("[BME] 0x%02X no ACK (Wire=%u)\n", addr, code); continue; }
        detectedAddress_ = addr;
        if (!readRegister(addr, 0xd0, chipId_)) {
            error_ = "Chip ID read failed";
            Serial.printf("[BME] 0x%02X ACK, chip ID read failed\n", addr);
            continue;
        }
        Serial.printf("[BME] 0x%02X ACK, chip ID=0x%02X (BME280 expects 0x60)\n", addr, chipId_);
        if (chipId_ != 0x60) { error_ = "Unexpected sensor chip ID"; continue; }
        if (bme_.begin(addr, &Wire)) {
            address_ = addr;
            error_ = "";
            break;
        }
        error_ = "BME initialization failed";
    }
    if (address_) bme_.setSampling(Adafruit_BME280::MODE_FORCED,
        Adafruit_BME280::SAMPLING_X1, Adafruit_BME280::SAMPLING_X1,
        Adafruit_BME280::SAMPLING_X1, Adafruit_BME280::FILTER_OFF);
}
Reading Sensor::measure() {
    if (!address_) begin();
    Reading r;
    r.address = detectedAddress_;
    r.chipId = chipId_;
    r.error = error_;
    if (!address_) return r;
    uint8_t id = 0;
    if (!readRegister(address_, 0xd0, id) || id != 0x60) {
        address_ = 0;
        r.error = "Sensor disconnected";
        return r;
    }
    if (!bme_.takeForcedMeasurement()) {
        address_ = 0;
        r.error = "Measurement timed out";
        return r;
    }
    // x1 T/P/RH conversion takes < 12 ms; also covers the status-bit start delay.
    delay(12);
    r.temperature = bme_.readTemperature();
    r.humidity = bme_.readHumidity();
    r.pressure = bme_.readPressure();
    r.valid = isfinite(r.temperature) && isfinite(r.humidity) && isfinite(r.pressure) &&
        r.temperature >= -40 && r.temperature <= 85 &&
        r.humidity >= 0 && r.humidity <= 100 && r.pressure >= 30000 && r.pressure <= 110000;
    if (!r.valid) {
        Serial.printf("[BME] invalid T=%f RH=%f P=%f\n", r.temperature, r.humidity, r.pressure);
        address_ = 0;
        r.error = "Invalid sensor values";
    } else r.error = "";
    return r;
}
void Sensor::diagnose() {
    Serial.println("[I2C] diagnostic: configured pins, then reversed data wires; no filesystem changes");
    lineFault_ = false;
    address_ = detectedAddress_ = chipId_ = 0;
    for (int swap = 0; swap < 2; ++swap) {
        Wire.end();
        int sda = swap ? PIN_I2C_SCL : PIN_I2C_SDA;
        int scl = swap ? PIN_I2C_SDA : PIN_I2C_SCL;
        if (!softwareProbe(sda, scl, error_)) { lineFault_ = true; return; }
        for (uint32_t hz : {100000UL, 10000UL}) {
            Wire.end();
            Wire.begin(sda, scl, hz);
            Wire.setTimeOut(15);
            Serial.printf("[I2C] SDA=%d SCL=%d clock=%lu levels=%d/%d\n", sda, scl,
                static_cast<unsigned long>(hz), digitalRead(sda), digitalRead(scl));
            unsigned found = 0, errors = 0;
            for (uint8_t addr = 8; addr < 120; ++addr) {
                Wire.beginTransmission(addr);
                uint8_t result = Wire.endTransmission();
                if (result == 0) {
                    ++found;
                    uint8_t id = 0;
                    bool read = readRegister(addr, 0xd0, id);
                    Serial.printf("[I2C] ACK 0x%02X chipID_read=%s chipID=0x%02X\n", addr, read ? "ok" : "failed", id);
                } else if (result != 2) {
                    if (++errors >= 4) { Serial.println("[I2C] scan stopped after repeated bus errors"); break; }
                }
                delay(1);
            }
            Serial.printf("[I2C] found=%u bus_errors=%u\n", found, errors);
        }
    }
    Wire.end();
    begin();
    Serial.println("[I2C] configured pins restored; diagnostic complete");
}
