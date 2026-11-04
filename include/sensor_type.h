#pragma once
#include <stdint.h>
enum class SensorType : uint8_t { None, Bme280, Bmp085180 };
inline SensorType sensorTypeFromId(uint8_t id) {
    return id == 0x60 ? SensorType::Bme280 : id == 0x55 ? SensorType::Bmp085180 : SensorType::None;
}
inline const char *sensorTypeName(SensorType type) {
    switch (type) {
    case SensorType::Bme280: return "BME280";
    case SensorType::Bmp085180: return "BMP085/180";
    default: return "unknown";
    }
}
