#include "settings.h"
#include <Preferences.h>
#include <ArduinoJson.h>
#include <esp_system.h>
bool updateWifiProfiles(Settings &next, const Settings &previous, JsonArrayConst profiles) {
    if (profiles.size() != WIFI_PROFILE_COUNT) return false;
    size_t i = 0;
    for (JsonVariantConst value : profiles) {
        if (!value.is<JsonObjectConst>() || !value["ssid"].is<const char *>() ||
            (value.containsKey("hidden") && !value["hidden"].is<bool>()) ||
            (value.containsKey("open") && !value["open"].is<bool>()) ||
            (value.containsKey("password") && !value["password"].is<const char *>())) return false;
        WifiProfile p;
        p.ssid = value["ssid"].as<String>();
        if (!p.ssid.isEmpty()) {
            bool found = false;
            for (const auto &old : previous.wifi) if (old.ssid == p.ssid) {
                p.password = old.password; p.hidden = old.hidden; found = true; break;
            }
            const String password = value["password"] | String("");
            const bool open = value["open"] | false;
            if (open && !password.isEmpty()) return false;
            if (open) p.password = "";
            else if (!password.isEmpty()) p.password = password;
            else if (!found) return false; // never reuse another SSID's secret
            if (value.containsKey("hidden")) p.hidden = value["hidden"];
        }
        next.wifi[i++] = p;
    }
    return validSettings(next);
}
bool validSettings(const Settings &s) {
    for (char c : s.adminPassword) if (c < 33 || c > 126) return false;
    for (const auto &p : s.wifi) {
        if (p.ssid.length() > 32 || p.password.length() > 63 ||
            (!p.password.isEmpty() && p.password.length() < 8) ||
            (p.ssid.isEmpty() && !p.password.isEmpty())) return false;
        if (!p.ssid.isEmpty()) for (const auto &q : s.wifi)
            if (&p != &q && p.ssid == q.ssid) return false;
    }
    return s.intervalS >= MIN_INTERVAL_S && s.intervalS <= MAX_INTERVAL_S &&
        s.screenTimeoutS >= 5 && s.screenTimeoutS <= 120 && s.brightness >= 1 &&
        s.adminPassword.length() >= 12 && s.adminPassword.length() <= 63;
}
bool saveSettings(const Settings &s) {
    if (!validSettings(s)) return false;
    DynamicJsonDocument doc(6144);
    doc["version"] = 2;
    doc["interval_s"] = s.intervalS;
    doc["screen_timeout_s"] = s.screenTimeoutS;
    doc["brightness"] = s.brightness;
    doc["auth_enabled"] = s.authEnabled;
    doc["auto_time"] = s.autoTime;
    JsonArray profiles = doc.createNestedArray("wifi_profiles");
    for (const auto &p : s.wifi) {
        JsonObject profile = profiles.createNestedObject();
        profile["ssid"] = p.ssid; profile["password"] = p.password; profile["hidden"] = p.hidden;
    }
    doc["admin_password"] = s.adminPassword;
    String json;
    serializeJson(doc, json);
    if (doc.overflowed() || json.length() > 4096) return false;
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
    DynamicJsonDocument doc(6144);
    if (json.length() > 4096 || deserializeJson(doc, json) ||
        (doc["version"] != 1 && doc["version"] != 2)) return false;
    s.intervalS = doc["interval_s"] | DEFAULT_INTERVAL_S;
    s.screenTimeoutS = doc["screen_timeout_s"] | 15;
    s.brightness = doc["brightness"] | 120;
    s.authEnabled = doc["auth_enabled"] | true;
    s.autoTime = doc["auto_time"] | true;
    for (auto &p : s.wifi) p = WifiProfile{};
    if (doc["version"] == 1) {
        // Migrate in RAM only; an unconfirmed OTA must not rewrite old settings.
        s.wifi[0].ssid = doc["ssid"].as<String>();
        s.wifi[0].password = doc["wifi_password"].as<String>();
        if (s.wifi[0].ssid.isEmpty()) s.wifi[0].password = "";
    } else {
        if (!doc["wifi_profiles"].is<JsonArray>() || doc["wifi_profiles"].size() != WIFI_PROFILE_COUNT) return false;
        size_t i = 0;
        for (JsonVariant p : doc["wifi_profiles"].as<JsonArray>()) {
            if (!p["ssid"].is<const char *>() || !p["password"].is<const char *>() || !p["hidden"].is<bool>()) return false;
            s.wifi[i].ssid = p["ssid"].as<String>(); s.wifi[i].password = p["password"].as<String>();
            s.wifi[i++].hidden = p["hidden"].as<bool>();
        }
    }
    s.adminPassword = doc["admin_password"].as<String>();
    return validSettings(s);
}
