#pragma once
#include <Arduino.h>
#include <vector>
#include <functional>
#include "record.h"
#include "clock_state.h"

struct HistoryBin {
    bool valid = false;
    bool gap = false;
    bool humidityValid = false, humidityGap = false;
    int16_t minT = 0, maxT = 0;
    uint16_t minH = 0, maxH = 0;
    uint32_t session = 0;
};
class Storage {
public:
    bool begin();
    bool append(logger::Record record);
    void visit(const std::function<bool(const logger::Record &)> &callback);
    void history(const ClockState &clock, uint32_t seconds, HistoryBin *bins, size_t count);
    bool latest(logger::Record &r);
    size_t used() const;
    size_t capacity() const;
    bool ready() const { return ready_; }
    const String &error() const { return error_; }
    uint32_t corruptRecords() const { return corrupt_; }
private:
    std::vector<uint32_t> blocks_;
    bool ready_ = false;
    String error_;
    uint32_t corrupt_ = 0;
    String path(uint32_t block) const;
    bool trimTail();
    bool retireOldest();
    void prune(const logger::Record &now);
};
