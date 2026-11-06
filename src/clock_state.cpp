#include "clock_state.h"
#include "config.h"
#include <Preferences.h>
#include <esp_sleep.h>
#include <esp_timer.h>
#include <sys/time.h>
#include <time.h>
namespace {
struct RetainedClock {
    uint32_t magic, session;
    uint64_t baseUs, deadlineUs;
    int64_t epochOffset, sleepWallUs;
    uint32_t crc;
};
RTC_DATA_ATTR RetainedClock rtc;
constexpr uint32_t MAGIC = 0x434c4b32;
uint64_t bootUs;
int64_t wallUs() {
    timeval tv;
    gettimeofday(&tv, nullptr);
    return static_cast<int64_t>(tv.tv_sec) * 1000000LL + tv.tv_usec;
}
uint32_t checksum() {
    return logger::crc16(reinterpret_cast<const uint8_t *>(&rtc), offsetof(RetainedClock, crc));
}
}
void ClockState::begin() {
    newSession_ = false;
    bootUs = esp_timer_get_time();
    if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_UNDEFINED &&
        rtc.magic == MAGIC && rtc.crc == checksum() && wallUs() >= rtc.sleepWallUs) {
        // Actual elapsed RTC time: a button may wake much earlier than the timer.
        rtc.baseUs = logger::resumeElapsed(rtc.baseUs, rtc.sleepWallUs, wallUs());
    } else {
        newSession_ = true;
        Preferences p;
        p.begin("clock", false);
        const uint32_t session = p.getUInt("session", 0) + 1;
        p.putUInt("session", session);
        p.end();
        rtc = {};
        rtc.magic = MAGIC;
        rtc.session = session;
    }
}
uint64_t ClockState::nowUs() const { return rtc.baseUs + esp_timer_get_time() - bootUs; }
uint32_t ClockState::session() const { return rtc.session; }
uint64_t ClockState::deadline() const { return rtc.deadlineUs; }
void ClockState::setDeadline(uint64_t us) { rtc.deadlineUs = us; }
uint32_t ClockState::epoch() const {
    return rtc.epochOffset ? static_cast<uint32_t>(rtc.epochOffset + elapsedS()) : 0;
}
void ClockState::sync(uint32_t value) {
    if (value < MIN_VALID_EPOCH || value > MAX_VALID_EPOCH) return;
    rtc.epochOffset = static_cast<int64_t>(value) - elapsedS();
    timeval tv = {static_cast<time_t>(value), 0};
    settimeofday(&tv, nullptr);
}
void ClockState::syncSystemTime() {
    time_t value = time(nullptr);
    if (value >= 1704067200 && (!epoch() || llabs(static_cast<int64_t>(value) - epoch()) > 2)) sync(value);
}
void ClockState::clearUtc() {
    rtc.epochOffset = 0;
    timeval tv = {0, 0};
    settimeofday(&tv, nullptr);
}
void ClockState::prepareSleep() {
    rtc.baseUs = nowUs();
    rtc.sleepWallUs = wallUs();
    rtc.crc = checksum();
}
