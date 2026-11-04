#include "service.h"
#include "web_ui.h"
#include "settings.h"
#include "ota_marker.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <esp_system.h>
#include <math.h>

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
    if (!server_.authenticate("admin", settings_.adminPassword.c_str())) {
        server_.requestAuthentication(BASIC_AUTH, "T-Display");
        return false;
    }
    if (write && (!power_.external() || server_.header("X-CSRF-Token") != csrf_)) {
        error(403, "External power and a valid session token are required");
        return false;
    }
    return true;
}
void Service::routes() {
    const char *headers[] = {"X-CSRF-Token"};
    server_.collectHeaders(headers, 1);
    server_.on("/", HTTP_GET, [this]() {
        if (!authorize()) return;
        server_.sendHeader("Content-Security-Policy", "default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; img-src 'self' blob:; connect-src 'self'; frame-ancestors 'none'; base-uri 'none'");
        server_.send_P(200, "text/html; charset=utf-8", WEB_UI);
    });
    server_.on("/api/status", HTTP_GET, [this]() { if (authorize()) status(); });
    server_.on("/api/history", HTTP_GET, [this]() { if (authorize()) history(); });
    server_.on("/api/export.csv", HTTP_GET, [this]() { if (authorize()) csv(); });
    server_.on("/api/config", HTTP_POST, [this]() { if (authorize(true)) configure(); });
    server_.on("/api/time", HTTP_POST, [this]() {
        if (!authorize(true)) return;
        StaticJsonDocument<128> doc;
        if (server_.arg("plain").length() > 128 || deserializeJson(doc, server_.arg("plain")) ||
            !doc["epoch"].is<uint32_t>() || doc["epoch"].as<uint32_t>() < MIN_VALID_EPOCH ||
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
        configTime(0, 0, "pool.ntp.org", "time.cloudflare.com");
        MDNS.begin(name_.c_str());
        MDNS.addService("http", "tcp", 80);
    }
    connected_ = connected;
    if (!ap_ && !connected && millis() - started_ > 15000) startAP();
    clock_.syncSystemTime();
    server_.handleClient();
    if (rebootAt_ && static_cast<int32_t>(millis() - rebootAt_) >= 0) ESP.restart();
}
void Service::status() {
    StaticJsonDocument<1536> doc;
    doc["version"] = FW_VERSION;
    doc["firmware_family"] = FIRMWARE_FAMILY;
    doc["variant"] = FORCE_SERVICE_MODE ? "bench-service" : USB_SENSE_WIRED ? "usb-sense" : "battery";
    doc["csrf"] = csrf_;
    doc["external"] = power_.external();
    doc["network"] = network();
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
    doc["used_bytes"] = storage_.used();
    doc["total_bytes"] = storage_.capacity();
    doc["storage_error"] = storage_.error();
    doc["ota_error"] = ota_.error();
    JsonObject config = doc.createNestedObject("config");
    config["interval_s"] = settings_.intervalS;
    config["screen_timeout_s"] = settings_.screenTimeoutS;
    config["brightness"] = settings_.brightness;
    config["ssid"] = settings_.ssid;
    String json; serializeJson(doc, json);
    server_.send(200, "application/json", json);
}
void Service::history() {
    const int hours = server_.hasArg("hours") ? server_.arg("hours").toInt() : 8;
    if (hours < 1 || hours > 720) { error(400, "hours must be 1..720"); return; }
    static HistoryBin bins[480];
    storage_.history(clock_, hours * 3600UL, bins, 480);
    server_.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server_.send(200, "application/json", "");
    server_.sendContent("{\"bins\":[");
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
            chunk += String(b.session); chunk += ']';
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
    const uint32_t session = clock_.session(), elapsed = clock_.elapsedS(), epoch = clock_.epoch();
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
        uint32_t age;
        if (hours && (!logger::ageSeconds(r, session, elapsed, epoch, age) || age >= hours * 3600UL)) return true;
        char row[160];
        logger::formatCsvRow(r, row, sizeof(row));
        chunk += row;
        if (chunk.length() >= 1024) { server_.sendContent(chunk); chunk = ""; }
        return true;
    });
    if (!complete) { server_.client().stop(); return; } // incomplete chunked transfer is detectable by client
    if (!chunk.isEmpty()) server_.sendContent(chunk);
    server_.sendContent("");
}
void Service::configure() {
    StaticJsonDocument<1536> doc;
    if (server_.arg("plain").length() > 1536 || deserializeJson(doc, server_.arg("plain"))) { error(400, "Invalid JSON"); return; }
    if (!doc["interval_s"].is<uint32_t>() || !doc["screen_timeout_s"].is<uint16_t>() || !doc["brightness"].is<uint8_t>() ||
        !doc["ssid"].is<const char *>()) { error(400, "Invalid configuration types"); return; }
    Settings next = settings_;
    next.intervalS = doc["interval_s"];
    next.screenTimeoutS = doc["screen_timeout_s"];
    next.brightness = doc["brightness"];
    next.ssid = doc["ssid"].as<String>();
    if (doc.containsKey("wifi_password")) {
        if (!doc["wifi_password"].is<const char *>()) { error(400, "Invalid Wi-Fi password"); return; }
        next.wifiPassword = doc["wifi_password"].as<String>();
    }
    if (doc.containsKey("admin_password")) {
        if (!doc["admin_password"].is<const char *>()) { error(400, "Invalid admin password"); return; }
        next.adminPassword = doc["admin_password"].as<String>();
    }
    if (doc["clear_wifi"] == true) { next.ssid = ""; next.wifiPassword = ""; }
    if (!validSettings(next)) { error(400, "Values outside allowed ranges"); return; }
    if (!saveSettings(next)) { error(500, "Cannot save settings"); return; }
    settings_ = next;
    server_.send(200, "application/json", "{\"ok\":true,\"wifi_restart_required\":true}");
}
void Service::upload() {
    HTTPUpload &u = server_.upload();
    if (u.status == UPLOAD_FILE_START) {
        uploadAuthorized_ = server_.authenticate("admin", settings_.adminPassword.c_str()) &&
            server_.header("X-CSRF-Token") == csrf_ && power_.external();
        uploadError_ = "";
        uploadOK_ = false;
        familySeen_ = false;
        familyMatch_ = 0;
        if (!uploadAuthorized_) return;
        if (u.name != "firmware" || !u.filename.endsWith(".bin")) { uploadError_ = "Select firmware.bin"; return; }
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
