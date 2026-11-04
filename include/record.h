#pragma once
#include <stdint.h>
#include <stddef.h>
namespace logger {
constexpr uint16_t FLAG_SENSOR_OK = 1;
constexpr uint16_t FLAG_UTC = 2;
constexpr uint16_t RECORD_VERSION = 0x1000;
#pragma pack(push, 1)
struct Record {
    uint32_t session;
    uint32_t elapsedS;
    uint32_t epoch;
    int16_t temperature; // hundredths of deg C
    uint16_t humidity;   // hundredths of % RH
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
    return (r.flags & 0xf000) == RECORD_VERSION &&
        r.crc == crc16(reinterpret_cast<const uint8_t *>(&r), offsetof(Record, crc));
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
