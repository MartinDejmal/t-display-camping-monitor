#include "network.h"
#include <WiFi.h>
#include <lwip/apps/sntp.h>
#include <esp_sntp.h>
#include <atomic>
#include <cmath>

namespace {
constexpr uint32_t ATTEMPT_MS = 15000, RETRY_MS = 60000, NTP_MS = 8000, BOOT_LIMIT_MS = 83000;
std::atomic<uint32_t> receivedEpoch{0};
void onNtp(timeval *value) {
    if (!value) return;
    const int64_t epoch = value->tv_sec;
    if (epoch >= static_cast<int64_t>(MIN_VALID_EPOCH) && epoch <= static_cast<int64_t>(MAX_VALID_EPOCH))
        receivedEpoch.store(static_cast<uint32_t>(epoch));
}
}
void WifiConnector::begin(const char *hostname) {
    WiFi.persistent(false); WiFi.mode(WIFI_STA); WiFi.setSleep(false); WiFi.setAutoReconnect(false);
    if (hostname) WiFi.setHostname(hostname);
    index_ = -1; exhausted_ = connected_ = false; next();
}
void WifiConnector::next() {
    while (++index_ < static_cast<int>(WIFI_PROFILE_COUNT)) {
        const auto &profile = settings_.wifi[index_];
        if (profile.ssid.isEmpty()) continue;
        WiFi.disconnect(false, false);
        // Directed connection also supports hidden SSIDs: do not filter by a scan.
        WiFi.begin(profile.ssid.c_str(), profile.password.c_str());
        attemptAt_ = millis(); return;
    }
    // USB service can still acquire a delayed DHCP lease on the last network
    // while waiting for the retry. Battery startup stops the radio on exit.
    exhausted_ = true; finishedAt_ = millis();
}
void WifiConnector::poll() {
    if (WiFi.status() == WL_CONNECTED) { connected_ = true; return; }
    if (connected_) { connected_ = false; index_ = -1; exhausted_ = false; next(); }
    else if (exhausted_) {
        if (millis() - finishedAt_ >= RETRY_MS) { index_ = -1; exhausted_ = false; next(); }
    } else if (millis() - attemptAt_ >= ATTEMPT_MS) next();
}
void WifiConnector::stop() {
    WiFi.scanDelete(); WiFi.disconnect(true, false); WiFi.mode(WIFI_OFF);
    connected_ = false;
}
BatteryTimeResult syncBatteryTime(const Settings &settings, ClockState &clock, Power &power) {
    if (!settings.autoTime) return BatteryTimeResult::Skipped;
    bool configured = false;
    for (const auto &p : settings.wifi) configured |= !p.ssid.isEmpty();
    if (!configured) return BatteryTimeResult::NoNetwork;
    float battery = power.batteryVoltage();
    if (!std::isfinite(battery) || battery < 3.50f) return BatteryTimeResult::LowBattery;
    WifiConnector wifi(settings);
    sntp_stop(); receivedEpoch.store(0); sntp_set_time_sync_notification_cb(onNtp);
    wifi.begin();
    const uint32_t started = millis();
    uint32_t ntpAt = 0, batteryAt = started;
    bool ntp = false;
    BatteryTimeResult result = BatteryTimeResult::NoNetwork;
    while (millis() - started < BOOT_LIMIT_MS) {
        power.poll();
        if (power.external()) { result = BatteryTimeResult::UsbConnected; break; }
        if (millis() - batteryAt >= 1000) {
            batteryAt = millis(); battery = power.batteryVoltage();
            if (!std::isfinite(battery) || battery < LOW_BATTERY_V) { result = BatteryTimeResult::LowBattery; break; }
        }
        wifi.poll();
        if (WiFi.status() == WL_CONNECTED && !ntp) {
            configTime(0, 0, "pool.ntp.org", "time.cloudflare.com");
            ntp = true; ntpAt = millis(); result = BatteryTimeResult::NtpTimeout;
        }
        const uint32_t epoch = receivedEpoch.load();
        if (epoch) { clock.sync(epoch); result = BatteryTimeResult::Synced; break; }
        if ((ntp && millis() - ntpAt >= NTP_MS) || (!ntp && wifi.exhausted())) break;
        delay(25);
    }
    sntp_stop(); sntp_set_time_sync_notification_cb(nullptr); wifi.stop();
    return result;
}
const char *batteryTimeResultName(BatteryTimeResult result) {
    switch (result) {
    case BatteryTimeResult::Synced: return "synchronized";
    case BatteryTimeResult::NoNetwork: return "no network";
    case BatteryTimeResult::NtpTimeout: return "NTP timeout";
    case BatteryTimeResult::LowBattery: return "low/unknown battery";
    case BatteryTimeResult::UsbConnected: return "USB connected";
    default: return "disabled";
    }
}
