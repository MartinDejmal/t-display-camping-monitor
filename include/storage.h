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
    uint32_t minP = 0, maxP = 0;
    uint32_t session = 0;
};
struct SessionSummary {
    uint32_t id = 0, count = 0, good = 0;
    uint32_t firstElapsed = 0, lastElapsed = 0;
    uint32_t firstEpoch = 0, lastEpoch = 0;
};
struct HistorySummary {
    uint32_t count = 0, good = 0, sessionCount = 0;
    logger::Record first{}, last{}, lastGood{};
    std::vector<SessionSummary> sessions; // latest 64, full CSV always retains all
};
class Storage {
public:
    bool begin();
    bool append(logger::Record record);
    void visit(const std::function<bool(const logger::Record &)> &callback);
    void history(const ClockState &clock, uint32_t seconds, HistoryBin *bins, size_t count);
    void historyForSession(uint32_t session, uint32_t referenceElapsed, uint32_t seconds, HistoryBin *bins, size_t count);
    const HistorySummary &summary();
    bool sessionInfo(uint32_t id, SessionSummary &session);
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
    HistorySummary summary_;
    bool summaryCached_ = false;
    void includeInSummary(const logger::Record &record);
    String path(uint32_t block) const;
    bool trimTail();
    bool retireOldest();
    void prune(const logger::Record &now);
};
