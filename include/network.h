#pragma once
#include "config.h"
#include "clock_state.h"
#include "power.h"

class WifiConnector {
public:
    explicit WifiConnector(const Settings &settings) : settings_(settings) {}
    void begin(const char *hostname = nullptr);
    void poll();
    void stop();
    bool exhausted() const { return exhausted_; }
    int index() const { return index_; }
private:
    const Settings &settings_;
    int index_ = -1;
    bool exhausted_ = false, connected_ = false;
    uint32_t attemptAt_ = 0, finishedAt_ = 0;
    void next();
};

enum class BatteryTimeResult { Skipped, Synced, NoNetwork, NtpTimeout, LowBattery, UsbConnected };
BatteryTimeResult syncBatteryTime(const Settings &settings, ClockState &clock, Power &power);
const char *batteryTimeResultName(BatteryTimeResult result);
