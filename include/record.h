#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include "sensor_type.h"
namespace logger {
constexpr uint16_t FLAG_SENSOR_OK = 1;
constexpr uint16_t FLAG_UTC = 2;
constexpr uint16_t FLAG_HUMIDITY = 4;
constexpr uint16_t TYPE_MASK = 0x30;
constexpr uint16_t TYPE_BME280 = 0x10;
constexpr uint16_t TYPE_BMP085180 = 0x20;
constexpr uint16_t LEGACY_RECORD_VERSION = 0x1000;
constexpr uint16_t RECORD_VERSION = 0x2000;
constexpr uint16_t HUMIDITY_UNAVAILABLE = 0xffff;
#pragma pack(push, 1)
struct Record {
    uint32_t session;
    uint32_t elapsedS;
    uint32_t epoch;
    int16_t temperature; // hundredths of deg C
    uint16_t humidity;   // hundredths of % RH, 0xffff when not available in v2
    uint32_t pressure;   // Pa
    uint16_t flags;
    uint16_t crc;
};
#pragma pack(pop)
static_assert(sizeof(Record) == 24, "Persistent record layout changed");
inline uint16_t crc16(const uint8_t *data, size_t n) {
    uint16_t crc = 0xffff;
    while (n--) {
        crc ^= static_cast<uint16_t>(*data++) << 8;
        for (int i = 0; i < 8; ++i)
            crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1;
    }
    return crc;
}
inline void seal(Record &r) {
    r.flags = (r.flags & 0xfff) | RECORD_VERSION;
    r.crc = crc16(reinterpret_cast<const uint8_t *>(&r), offsetof(Record, crc));
}
inline bool valid(const Record &r) {
    const uint16_t version = r.flags & 0xf000;
    return (version == RECORD_VERSION || version == LEGACY_RECORD_VERSION) &&
        r.crc == crc16(reinterpret_cast<const uint8_t *>(&r), offsetof(Record, crc));
}
inline bool hasHumidity(const Record &r) {
    return (r.flags & FLAG_SENSOR_OK) && r.humidity <= 10000 &&
        ((r.flags & 0xf000) == LEGACY_RECORD_VERSION || (r.flags & FLAG_HUMIDITY));
}
inline uint16_t typeFlags(SensorType type) {
    return type == SensorType::Bme280 ? TYPE_BME280 : type == SensorType::Bmp085180 ? TYPE_BMP085180 : 0;
}
inline SensorType recordSensorType(const Record &r) {
    if ((r.flags & 0xf000) == LEGACY_RECORD_VERSION)
        return r.flags & FLAG_SENSOR_OK ? SensorType::Bme280 : SensorType::None;
    return (r.flags & TYPE_MASK) == TYPE_BME280 ? SensorType::Bme280 :
        (r.flags & TYPE_MASK) == TYPE_BMP085180 ? SensorType::Bmp085180 : SensorType::None;
}
constexpr const char *CSV_HEADER = "session,elapsed_s,utc_epoch,temperature_c,humidity_pct,pressure_pa,sensor_ok,humidity_available,sensor_type\r\n";
inline int formatCsvRow(const Record &r, char *out, size_t size) {
    char epoch[16] = {}, temperature[16] = {}, humidity[16] = {}, pressure[16] = {};
    if (r.flags & FLAG_UTC) snprintf(epoch, sizeof(epoch), "%lu", static_cast<unsigned long>(r.epoch));
    if (r.flags & FLAG_SENSOR_OK) {
        snprintf(temperature, sizeof(temperature), "%.2f", r.temperature / 100.0);
        snprintf(pressure, sizeof(pressure), "%lu", static_cast<unsigned long>(r.pressure));
        if (hasHumidity(r)) snprintf(humidity, sizeof(humidity), "%.2f", r.humidity / 100.0);
    }
    return snprintf(out, size, "%lu,%lu,%s,%s,%s,%s,%u,%u,%s\r\n",
        static_cast<unsigned long>(r.session), static_cast<unsigned long>(r.elapsedS), epoch,
        temperature, humidity, pressure, (r.flags & FLAG_SENSOR_OK) ? 1 : 0,
        hasHumidity(r) ? 1 : 0, sensorTypeName(recordSensorType(r)));
}
inline uint64_t advanceDeadline(uint64_t deadline, uint64_t now, uint32_t intervalS) {
    const uint64_t period = static_cast<uint64_t>(intervalS) * 1000000ULL;
    if (!period || deadline > now) return deadline;
    return deadline + ((now - deadline) / period + 1) * period;
}
inline uint64_t resumeElapsed(uint64_t savedUs, int64_t sleepWallUs, int64_t wakeWallUs) {
    return wakeWallUs >= sleepWallUs ? savedUs + static_cast<uint64_t>(wakeWallUs - sleepWallUs) : savedUs;
}
inline bool ageSeconds(const Record &r, uint32_t session, uint32_t elapsed,
                       uint32_t epoch, uint32_t &age) {
    if (r.session == session && r.elapsedS <= elapsed) {
        age = elapsed - r.elapsedS;
        return true;
    }
    if ((r.flags & FLAG_UTC) && epoch >= r.epoch && r.epoch) {
        age = epoch - r.epoch;
        return true;
    }
    return false;
}
} // namespace logger
