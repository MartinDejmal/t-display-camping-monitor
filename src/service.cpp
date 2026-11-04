#include "service.h"
#include "web_ui.h"
#include "settings.h"
#include "ota_marker.h"
#include "access_policy.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <esp_system.h>
#include <esp_heap_caps.h>
#include <math.h>
#include <lwip/apps/sntp.h>

Service::Service(Settings &s, ClockState &c, Storage &l, Power &p, OtaGuard &o, Reading &r)
    : settings_(s), clock_(c), storage_(l), power_(p), ota_(o), reading_(r) {}
void Service::error(int code, const char *message) {
    StaticJsonDocument<256> d;
    d["error"] = message;
    String json; serializeJson(d, json);
    server_.send(code, "application/json", json);
}
bool Service::authorize(bool write) {
    server_.sendHeader("Cache-Control", "no-store");
    server_.sendHeader("X-Content-Type-Options", "nosniff");
    if (!loginAllowed(settings_.authEnabled, server_.authenticate("admin", settings_.adminPassword.c_str()))) {
        server_.requestAuthentication(BASIC_AUTH, "T-Display");
        return false;
    }
    if (write && !writeAllowed(power_.external(), server_.header("X-CSRF-Token") == csrf_)) {
        error(403, "External power and a valid session token are required");
        return false;
    }
    return true;
}
void Service::routes() {
    const char *headers[] = {"X-CSRF-Token"};
    server_.collectHeaders(headers, 1);
    server_.on("/", HTTP_GET, [this]() { if (authorize()) page("overview", "Přehled", PAGE_OVERVIEW); });
    server_.on("/history", HTTP_GET, [this]() { if (authorize()) page("history", "Historie", PAGE_HISTORY); });
    server_.on("/device", HTTP_GET, [this]() { if (authorize()) page("device", "Zařízení", PAGE_DEVICE); });
    server_.on("/settings", HTTP_GET, [this]() { if (authorize()) page("settings", "Nastavení", PAGE_SETTINGS); });
    server_.on("/time", HTTP_GET, [this]() { if (authorize()) page("time", "Čas", PAGE_TIME); });
    server_.on("/firmware", HTTP_GET, [this]() { if (authorize()) page("firmware", "Firmware", PAGE_FIRMWARE); });
    server_.on("/assets/app.css", HTTP_GET, [this]() { if (authorize()) server_.send_P(200, "text/css; charset=utf-8", WEB_CSS); });
    server_.on("/assets/app.js", HTTP_GET, [this]() { if (authorize()) server_.send_P(200, "application/javascript; charset=utf-8", WEB_JS); });
    server_.on("/api/status", HTTP_GET, [this]() { if (authorize()) status(); });
    server_.on("/api/history", HTTP_GET, [this]() { if (authorize()) history(); });
    server_.on("/api/sessions", HTTP_GET, [this]() { if (authorize()) sessions(); });
    server_.on("/api/export.csv", HTTP_GET, [this]() { if (authorize()) csv(); });
    server_.on("/api/config", HTTP_POST, [this]() { if (authorize(true)) configure(); });
    server_.on("/api/time", HTTP_POST, [this]() {
        if (!authorize(true)) return;
        StaticJsonDocument<128> doc;
        if (server_.arg("plain").length() > 128 || deserializeJson(doc, server_.arg("plain"))) { error(400, "Invalid time request"); return; }
        if (doc["clear"] == true) {
            Settings next = settings_;
            next.autoTime = false;
            if (!saveSettings(next)) { error(500, "Cannot save time settings"); return; }
            settings_ = next;
            sntp_stop(); ntpStarted_ = false;
            clock_.clearUtc();
            server_.send(200, "application/json", "{\"ok\":true,\"time_known\":false}");
            return;
        }
        if (!doc["epoch"].is<uint32_t>() || doc["epoch"].as<uint32_t>() < MIN_VALID_EPOCH ||
            doc["epoch"].as<uint32_t>() > MAX_VALID_EPOCH) { error(400, "Invalid UTC epoch"); return; }
        clock_.sync(doc["epoch"]);
        server_.send(200, "application/json", "{\"ok\":true}");
    });
    server_.on("/api/restart", HTTP_POST, [this]() {
        if (!authorize(true) || uploading_) return;
        server_.send(200, "application/json", "{\"ok\":true}");
        rebootAt_ = millis() + 1000;
    });
    server_.on("/api/update", HTTP_POST, [this]() { uploadFinished(); }, [this]() { upload(); });
    server_.onNotFound([this]() { if (authorize()) error(404, "Not found"); });
    routes_ = true;
}
void Service::page(const char *id, const char *title, const char *body) {
    server_.sendHeader("Content-Security-Policy", "default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self' blob:; connect-src 'self'; frame-ancestors 'none'; base-uri 'none'");
    server_.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server_.send(200, "text/html; charset=utf-8", "");
    server_.sendContent_P(WEB_HEAD);
    server_.sendContent(String("<body data-page=\"") + id + "\"><header><a class=brand href=\"/\">T-Display</a><span>Monitor prostředí</span></header><nav aria-label=\"Hlavní navigace\">");
    struct Nav { const char *id, *path, *title; };
    const Nav nav[] = {{"overview","/","Přehled"},{"history","/history","Historie"},{"device","/device","Zařízení"},
        {"settings","/settings","Nastavení"},{"time","/time","Čas"},{"firmware","/firmware","Firmware"}};
    for (const auto &item : nav) server_.sendContent(String("<a href=\"") + item.path + "\"" +
        (strcmp(id, item.id) == 0 ? " aria-current=\"page\"" : "") + ">" + item.title + "</a>");
    server_.sendContent(String("</nav><main><h1>") + title + "</h1><div id=message role=status aria-live=polite></div>");
    server_.sendContent_P(body);
    server_.sendContent_P(WEB_TAIL);
    server_.sendContent("");
}
void Service::begin() {
    if (running_ || settings_.adminPassword.isEmpty()) return;
    char suffix[7];
    snprintf(suffix, sizeof(suffix), "%06lx", static_cast<unsigned long>(ESP.getEfuseMac() & 0xffffff));
    name_ = String("tdisplay-") + suffix;
    char token[33];
    snprintf(token, sizeof(token), "%08lx%08lx%08lx%08lx", static_cast<unsigned long>(esp_random()),
        static_cast<unsigned long>(esp_random()), static_cast<unsigned long>(esp_random()), static_cast<unsigned long>(esp_random()));
    csrf_ = token;
    if (!routes_) routes();
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    // Service runs on external power: keep the radio responsive for HTTP/OTA.
    // Battery mode stops Wi-Fi entirely before deep sleep.
    WiFi.setSleep(false);
    WiFi.setHostname(name_.c_str());
    if (settings_.ssid.isEmpty()) startAP();
    else WiFi.begin(settings_.ssid.c_str(), settings_.wifiPassword.c_str());
    started_ = millis();
    server_.begin();
    running_ = true;
}
void Service::startAP() {
    WiFi.mode(WIFI_AP_STA);
    ap_ = WiFi.softAP(name_.c_str(), settings_.adminPassword.c_str(), 1, false, 2);
}
void Service::stop() {
    if (uploading_) return;
    server_.stop();
    MDNS.end();
    sntp_stop(); ntpStarted_ = false;
    WiFi.disconnect(true, false);
    WiFi.mode(WIFI_OFF);
    running_ = ap_ = connected_ = false;
}
bool Service::available() const { return ap_ || WiFi.status() == WL_CONNECTED; }
String Service::network() const { return ap_ ? name_ : WiFi.status() == WL_CONNECTED ? WiFi.SSID() : "Connecting Wi-Fi..."; }
String Service::ip() const { return ap_ ? WiFi.softAPIP().toString() : WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "Waiting for network"; }
void Service::poll() {
    if (!running_) return;
    bool connected = WiFi.status() == WL_CONNECTED;
    if (connected && !connected_) {
        MDNS.begin(name_.c_str());
        MDNS.addService("http", "tcp", 80);
    }
    connected_ = connected;
    if (connected && settings_.autoTime && !ntpStarted_) {
        configTime(0, 0, "pool.ntp.org", "time.cloudflare.com");
        ntpStarted_ = true;
    } else if ((!settings_.autoTime || !connected) && ntpStarted_) {
        sntp_stop(); ntpStarted_ = false;
    }
    if (!ap_ && !connected && millis() - started_ > 15000) startAP();
    if (ntpStarted_) clock_.syncSystemTime();
    server_.handleClient();
    if (rebootAt_ && static_cast<int32_t>(millis() - rebootAt_) >= 0) ESP.restart();
}
void Service::status() {
    DynamicJsonDocument doc(4096);
    doc["version"] = FW_VERSION;
    doc["firmware_family"] = FIRMWARE_FAMILY;
    doc["variant"] = FORCE_SERVICE_MODE ? "bench-service" : USB_SENSE_WIRED ? "usb-sense" : "battery";
    doc["uptime_s"] = millis() / 1000;
    doc["free_heap_bytes"] = ESP.getFreeHeap();
    doc["min_free_heap_bytes"] = ESP.getMinFreeHeap();
    doc["largest_free_block_bytes"] = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    doc["loop_stack_free_min_bytes"] = uxTaskGetStackHighWaterMark(nullptr);
    doc["reset_reason"] = static_cast<unsigned>(esp_reset_reason());
    doc["csrf"] = csrf_;
    doc["external"] = power_.external();
    doc["power_mode"] = FORCE_SERVICE_MODE ? "bench" : power_.external() ? "usb" : "battery";
    float battery = power_.batteryVoltage();
    if (isfinite(battery)) doc["battery_v"] = battery;
    else doc["battery_v"] = nullptr;
    doc["network"] = network();
    doc["lan_ip"] = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : String("");
    doc["ap_ip"] = ap_ ? WiFi.softAPIP().toString() : String("");
    doc["sensor_ok"] = reading_.valid;
    doc["sensor_error"] = reading_.error;
    doc["sensor_address"] = reading_.address;
    doc["sensor_chip_id"] = reading_.chipId;
    doc["sensor_type"] = sensorTypeName(reading_.type);
    doc["has_humidity"] = reading_.humidityAvailable;
    doc["humidity"] = nullptr;
    if (reading_.valid) {
        doc["temperature"] = reading_.temperature;
        if (reading_.humidityAvailable) doc["humidity"] = reading_.humidity;
        doc["pressure_hpa"] = reading_.pressure / 100;
    }
    doc["session"] = clock_.session();
    doc["elapsed_s"] = clock_.elapsedS();
    doc["epoch"] = clock_.epoch();
    doc["time_known"] = clock_.epoch() != 0;
    doc["time_basis"] = "session_elapsed";
    doc["interval_s"] = settings_.intervalS;
    uint64_t now = clock_.nowUs(), due = clock_.deadline();
    doc["next_sample_in_s"] = due > now ? (due - now + 999999) / 1000000 : 0;
    doc["used_bytes"] = storage_.used();
    doc["total_bytes"] = storage_.capacity();
    doc["storage_error"] = storage_.error();
    doc["ota_error"] = ota_.error();
    const auto &history = storage_.summary();
    doc["record_count"] = history.count;
    doc["valid_record_count"] = history.good;
    doc["session_count"] = history.sessionCount;
    auto location = [&](const char *name, const logger::Record &r, bool present) {
        if (!present) { doc[name] = nullptr; return; }
        JsonObject obj = doc.createNestedObject(name);
        obj["session"] = r.session;
        obj["elapsed_s"] = r.elapsedS;
        obj["epoch"] = r.flags & logger::FLAG_UTC ? r.epoch : 0;
    };
    location("first_record", history.first, history.count != 0);
    location("last_write", history.last, history.count != 0);
    location("last_measurement", history.lastGood, history.good != 0);
    doc["last_write_age_s"] = nullptr;
    if (history.count && history.last.session == clock_.session() && history.last.elapsedS <= clock_.elapsedS())
        doc["last_write_age_s"] = clock_.elapsedS() - history.last.elapsedS;
    JsonObject config = doc.createNestedObject("config");
    config["interval_s"] = settings_.intervalS;
    config["screen_timeout_s"] = settings_.screenTimeoutS;
    config["brightness"] = settings_.brightness;
    config["ssid"] = settings_.ssid;
    config["auth_enabled"] = settings_.authEnabled;
    config["auto_time"] = settings_.autoTime;
    String json; serializeJson(doc, json);
    server_.send(200, "application/json", json);
}
void Service::sessions() {
    const auto &summary = storage_.summary();
    DynamicJsonDocument doc(16384);
    doc["total"] = summary.sessionCount;
    doc["current"] = clock_.session();
    doc["time_basis"] = "session_elapsed";
    JsonArray list = doc.createNestedArray("sessions");
    for (auto it = summary.sessions.rbegin(); it != summary.sessions.rend(); ++it) {
        JsonObject s = list.createNestedObject();
        s["id"] = it->id; s["count"] = it->count; s["good"] = it->good;
        s["first_elapsed_s"] = it->firstElapsed; s["last_elapsed_s"] = it->lastElapsed;
        s["first_epoch"] = it->firstEpoch; s["last_epoch"] = it->lastEpoch;
    }
    String json; serializeJson(doc, json);
    server_.send(200, "application/json", json);
}
bool Service::historyReference(uint32_t &session, uint32_t &elapsed) {
    session = server_.hasArg("session") ? server_.arg("session").toInt() : clock_.session();
    if (!session) { error(400, "Invalid session"); return false; }
    if (session == clock_.session()) { elapsed = clock_.elapsedS(); return true; }
    SessionSummary info;
    if (!storage_.sessionInfo(session, info)) { error(404, "Session not listed; full CSV includes older records"); return false; }
    elapsed = info.lastElapsed;
    return true;
}
void Service::history() {
    const int hours = server_.hasArg("hours") ? server_.arg("hours").toInt() : 8;
    if (hours < 1 || hours > 720) { error(400, "hours must be 1..720"); return; }
    static HistoryBin bins[480];
    uint32_t session, reference;
    if (!historyReference(session, reference)) return;
    storage_.historyForSession(session, reference, hours * 3600UL, bins, 480);
    server_.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server_.send(200, "application/json", "");
    server_.sendContent(String("{\"time_basis\":\"session_elapsed\",\"session\":") + session +
        ",\"reference_elapsed_s\":" + reference + ",\"bins\":[");
    String chunk;
    for (size_t i = 0; i < 480; ++i) {
        if (i) chunk += ',';
        auto &b = bins[i];
        if (!b.valid || b.gap) chunk += "null";
        else {
            chunk += '['; chunk += String(b.minT / 100.0f, 2); chunk += ',';
            chunk += String(b.maxT / 100.0f, 2); chunk += ',';
            bool humidity = b.humidityValid && !b.humidityGap;
            chunk += humidity ? String(b.minH / 100.0f, 2) : String("null"); chunk += ',';
            chunk += humidity ? String(b.maxH / 100.0f, 2) : String("null"); chunk += ',';
            chunk += String(b.session); chunk += ',';
            chunk += String(b.minP / 100.0f, 2); chunk += ',';
            chunk += String(b.maxP / 100.0f, 2); chunk += ']';
        }
        if (chunk.length() > 1024) { server_.sendContent(chunk); chunk = ""; if (tick_) tick_(); }
    }
    server_.sendContent(chunk + "]}");
    server_.sendContent("");
}
void Service::csv() {
    if (!storage_.ready()) { error(503, "History unavailable"); return; }
    const uint32_t hours = server_.hasArg("hours") ? server_.arg("hours").toInt() : 0;
    if (hours > 720 || (server_.hasArg("hours") && !hours)) { error(400, "hours must be 1..720 or omitted"); return; }
    uint32_t session = 0, reference = 0;
    const bool filtered = hours || server_.hasArg("session");
    if (filtered && !historyReference(session, reference)) return;
    server_.sendHeader("Content-Disposition", "attachment; filename=tdisplay-history.csv");
    server_.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server_.send(200, "text/csv; charset=utf-8", "");
    server_.sendContent(logger::CSV_HEADER);
    String chunk;
    uint32_t visited = 0, started = millis();
    bool complete = true;
    storage_.visit([&](const logger::Record &r) {
        if ((++visited & 63) == 0 && tick_) tick_();
        if (!server_.client().connected() || millis() - started > 60000) { complete = false; return false; }
        if (filtered && r.session != session) return true;
        if (hours && (r.elapsedS > reference || reference - r.elapsedS >= hours * 3600UL)) return true;
        char row[160];
        logger::formatCsvRow(r, row, sizeof(row));
        chunk += row;
        if (chunk.length() >= 1024) { server_.sendContent(chunk); chunk = ""; }
        return true;
    });
    if (!complete) { server_.abortClient(); return; } // incomplete chunked transfer is detectable by client
    if (!chunk.isEmpty()) server_.sendContent(chunk);
    server_.sendContent("");
}
void Service::configure() {
    StaticJsonDocument<1536> doc;
    if (server_.arg("plain").length() > 1536 || deserializeJson(doc, server_.arg("plain"))) { error(400, "Invalid JSON"); return; }
    Settings next = settings_;
    bool known = false;
    if (doc.containsKey("interval_s")) {
        if (!doc["interval_s"].is<uint32_t>()) { error(400, "Invalid interval"); return; }
        next.intervalS = doc["interval_s"]; known = true;
    }
    if (doc.containsKey("screen_timeout_s")) {
        if (!doc["screen_timeout_s"].is<uint16_t>()) { error(400, "Invalid timeout"); return; }
        next.screenTimeoutS = doc["screen_timeout_s"]; known = true;
    }
    if (doc.containsKey("brightness")) {
        if (!doc["brightness"].is<uint8_t>()) { error(400, "Invalid brightness"); return; }
        next.brightness = doc["brightness"]; known = true;
    }
    if (doc.containsKey("ssid")) {
        if (!doc["ssid"].is<const char *>()) { error(400, "Invalid SSID"); return; }
        next.ssid = doc["ssid"].as<String>(); known = true;
    }
    if (doc.containsKey("auth_enabled")) {
        if (!doc["auth_enabled"].is<bool>()) { error(400, "Invalid authentication switch"); return; }
        next.authEnabled = doc["auth_enabled"]; known = true;
    }
    if (doc.containsKey("auto_time")) {
        if (!doc["auto_time"].is<bool>()) { error(400, "Invalid NTP switch"); return; }
        next.autoTime = doc["auto_time"]; known = true;
    }
    if (doc.containsKey("wifi_password")) {
        if (!doc["wifi_password"].is<const char *>()) { error(400, "Invalid Wi-Fi password"); return; }
        next.wifiPassword = doc["wifi_password"].as<String>();
        known = true;
    }
    if (doc.containsKey("admin_password")) {
        if (!doc["admin_password"].is<const char *>()) { error(400, "Invalid admin password"); return; }
        next.adminPassword = doc["admin_password"].as<String>();
        known = true;
    }
    if (doc["clear_wifi"] == true) { next.ssid = ""; next.wifiPassword = ""; known = true; }
    if (!known) { error(400, "No configuration fields provided"); return; }
    if (!validSettings(next)) { error(400, "Values outside allowed ranges"); return; }
    if (!saveSettings(next)) { error(500, "Cannot save settings"); return; }
    settings_ = next;
    server_.send(200, "application/json", "{\"ok\":true,\"wifi_restart_required\":true}");
}
void Service::upload() {
    HTTPUpload &u = server_.upload();
    if (u.status == UPLOAD_FILE_START) {
        if (updating() || ota_.trial()) {
            uploadAuthorized_ = false;
            error(409, "Firmware update or trial boot already in progress");
            server_.abortClient(); return;
        }
        uploadAuthorized_ = loginAllowed(settings_.authEnabled, server_.authenticate("admin", settings_.adminPassword.c_str())) &&
            writeAllowed(power_.external(), server_.header("X-CSRF-Token") == csrf_);
        uploadError_ = "";
        uploadOK_ = false;
        familySeen_ = false;
        familyMatch_ = 0;
        if (!uploadAuthorized_) {
            error(403, "Authentication, USB power and session token required");
            server_.abortClient(); return;
        }
        Serial.println("OTA: upload started");
        if (u.name != "firmware" || !u.filename.endsWith(".bin")) {
            error(400, "Select firmware.bin"); server_.abortClient(); return;
        }
        if (!ota_.stage()) { uploadError_ = "Cannot arm OTA guard"; return; }
        uploading_ = Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH);
        if (!uploading_) { uploadError_ = Update.errorString(); ota_.cancel(); }
    } else if (u.status == UPLOAD_FILE_WRITE) {
        if (!uploadAuthorized_ || !uploading_) return;
        feedOtaMarker(u.buf, u.currentSize, FIRMWARE_FAMILY, familyMatch_, familySeen_);
        power_.poll();
        if (!power_.external()) {
            const float voltage = power_.batteryVoltage();
            if (!isfinite(voltage) || voltage < OTA_MIN_BATTERY_V) {
                uploadError_ = "USB removed and battery too low";
                Update.abort(); uploading_ = false; ota_.cancel(); return;
            }
        }
        if (Update.write(u.buf, u.currentSize) != u.currentSize) {
            uploadError_ = Update.errorString(); Update.abort(); uploading_ = false; ota_.cancel();
        }
        yield();
    } else if (u.status == UPLOAD_FILE_END) {
        if (!uploading_) return;
        if (!familySeen_) {
            uploadError_ = "Incompatible firmware family, wiring variant or data layout";
            Update.abort(); uploading_ = false; ota_.cancel(); return;
        }
        uploadOK_ = Update.end(true);
        Serial.printf("OTA: upload %s (%u bytes)\n", uploadOK_ ? "accepted" : "failed", static_cast<unsigned>(u.totalSize));
        uploading_ = false;
        if (!uploadOK_) { uploadError_ = Update.errorString(); ota_.cancel(); }
    } else if (u.status == UPLOAD_FILE_ABORTED) {
        if (uploading_) Update.abort();
        uploading_ = false; uploadOK_ = false; uploadError_ = "Upload interrupted";
        if (uploadAuthorized_) ota_.cancel();
    }
}
void Service::uploadFinished() {
    if (!uploadAuthorized_) { error(403, "Authentication, USB power and session token required"); return; }
    if (!uploadOK_) { error(400, uploadError_.isEmpty() ? "No firmware received" : uploadError_.c_str()); return; }
    server_.send(200, "application/json", "{\"ok\":true,\"restarting\":true}");
    rebootAt_ = millis() + 1000;
}
