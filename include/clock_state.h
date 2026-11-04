#pragma once
#include <Arduino.h>
#include "record.h"
struct ClockState {
    void begin();
    uint64_t nowUs() const;
    uint32_t elapsedS() const { return nowUs() / 1000000ULL; }
    uint32_t epoch() const;
    uint32_t session() const;
    uint64_t deadline() const;
    void setDeadline(uint64_t us);
    void sync(uint32_t epoch);
    void clearUtc();
    void syncSystemTime();
    void prepareSleep();
};
