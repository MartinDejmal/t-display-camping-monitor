#include "settings.h"
#include <Preferences.h>
#include <ArduinoJson.h>
#include <esp_system.h>
bool validSettings(const Settings &s) {
    for (char c : s.adminPassword) if (c < 33 || c > 126) return false;
    return s.intervalS >= MIN_INTERVAL_S && s.intervalS <= MAX_INTERVAL_S &&
        s.screenTimeoutS >= 5 && s.screenTimeoutS <= 120 && s.brightness >= 1 &&
        s.ssid.length() <= 32 && s.wifiPassword.length() <= 63 &&
        (s.wifiPassword.isEmpty() || s.wifiPassword.length() >= 8) &&
        s.adminPassword.length() >= 12 && s.adminPassword.length() <= 63;
}
bool saveSettings(const Settings &s) {
    if (!validSettings(s)) return false;
    StaticJsonDocument<768> doc;
    doc["version"] = 1;
    doc["interval_s"] = s.intervalS;
    doc["screen_timeout_s"] = s.screenTimeoutS;
    doc["brightness"] = s.brightness;
    doc["auth_enabled"] = s.authEnabled;
    doc["auto_time"] = s.autoTime;
    doc["ssid"] = s.ssid;
    doc["wifi_password"] = s.wifiPassword;
    doc["admin_password"] = s.adminPassword;
    String json;
    serializeJson(doc, json);
    Preferences p;
    if (!p.begin("settings", false)) return false;
    bool ok = p.putString("json", json) == json.length();
    p.end();
    return ok;
}
bool loadSettings(Settings &s) {
    Preferences p;
    if (!p.begin("settings", false)) return false;
    String json = p.getString("json", "");
    p.end();
    if (json.isEmpty()) {
        char password[17];
        snprintf(password, sizeof(password), "%08lx%08lx", static_cast<unsigned long>(esp_random()),
                 static_cast<unsigned long>(esp_random()));
        s.adminPassword = password;
        return saveSettings(s);
    }
    StaticJsonDocument<768> doc;
    if (deserializeJson(doc, json) || doc["version"] != 1) return false;
    s.intervalS = doc["interval_s"] | DEFAULT_INTERVAL_S;
    s.screenTimeoutS = doc["screen_timeout_s"] | 15;
    s.brightness = doc["brightness"] | 120;
    s.authEnabled = doc["auth_enabled"] | true;
    s.autoTime = doc["auto_time"] | true;
    s.ssid = doc["ssid"].as<String>();
    s.wifiPassword = doc["wifi_password"].as<String>();
    s.adminPassword = doc["admin_password"].as<String>();
    return validSettings(s);
}
