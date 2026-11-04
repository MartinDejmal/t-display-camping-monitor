#pragma once
#define RTC_DATA_ATTR
enum esp_sleep_wakeup_cause_t { ESP_SLEEP_WAKEUP_UNDEFINED, ESP_SLEEP_WAKEUP_TIMER, ESP_SLEEP_WAKEUP_EXT0 };
extern esp_sleep_wakeup_cause_t fakeWake;
inline esp_sleep_wakeup_cause_t esp_sleep_get_wakeup_cause() { return fakeWake; }
