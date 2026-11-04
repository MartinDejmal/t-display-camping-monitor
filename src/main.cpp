#include <Arduino.h>
#include <WiFi.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>
#include <math.h>
#include "config.h"
#include "pins.h"
#include "settings.h"
#include "clock_state.h"
#include "storage.h"
#include "sensor.h"
#include "power.h"
#include "display.h"
#include "service.h"
#include "ota_guard.h"

namespace {
Settings settings;
ClockState clockState;
Storage storage;
Sensor sensor;
Power power;
Display display;
OtaGuard ota;
Reading reading;
Service service(settings, clockState, storage, power, ota, reading);
float battery = NAN;
bool settingsOK = false;
uint32_t lastActivity = 0, lastLive = 0, lastServiceDraw = 0;
uint32_t appliedInterval = DEFAULT_INTERVAL_S;
enum class View { Values, Graph, Service };
View view = View::Values;

class Button {
public:
    explicit Button(int pin) : pin_(pin) {}
    void begin() { stable_ = candidate_ = digitalRead(pin_) == LOW; changed_ = millis(); }
    bool pressed() {
        const bool next = digitalRead(pin_) == LOW;
        if (next != candidate_) { candidate_ = next; changed_ = millis(); }
        if (candidate_ != stable_ && millis() - changed_ >= 35) {
            stable_ = candidate_;
            return stable_;
        }
        return false;
    }
private:
    int pin_;
    bool stable_ = false, candidate_ = false;
    uint32_t changed_ = 0;
};
Button right(PIN_BTN_RIGHT), left(PIN_BTN_LEFT);

void render() {
    if (!display.awake()) return;
    switch (view) {
    case View::Graph: display.graph(storage, clockState); break;
    case View::Service:
        if (service.running()) display.service(service.network(), service.ip(), settings.adminPassword);
        else display.message("Service unavailable", USB_SENSE_WIRED ? "Connect USB power" : "USB sense not fitted");
        break;
    default: display.values(reading, battery, power.external(), storage); break;
    }
}
void sampleDue() {
    if (service.updating()) return;
    if (settings.intervalS != appliedInterval) {
        appliedInterval = settings.intervalS;
        clockState.setDeadline(clockState.nowUs() + static_cast<uint64_t>(appliedInterval) * 1000000ULL);
    }
    const uint64_t now = clockState.nowUs();
    if (clockState.deadline() && now < clockState.deadline()) return;
    battery = power.batteryVoltage();
    const bool lowBattery = !power.external() && isfinite(battery) && battery < LOW_BATTERY_V;
    if (!lowBattery) {
        reading = sensor.measure();
        logger::Record r{};
        r.session = clockState.session();
        r.elapsedS = clockState.elapsedS();
        r.epoch = clockState.epoch();
        if (r.epoch) r.flags |= logger::FLAG_UTC;
        if (reading.valid) {
            r.flags |= logger::FLAG_SENSOR_OK;
            r.temperature = lroundf(reading.temperature * 100);
            r.humidity = lroundf(reading.humidity * 100);
            r.pressure = lroundf(reading.pressure);
        }
        const bool saved = storage.append(r);
        Serial.printf("sample session=%lu elapsed=%lu sensor=%s log=%s\n",
            static_cast<unsigned long>(r.session), static_cast<unsigned long>(r.elapsedS),
            reading.valid ? "ok" : "missing", saved ? "ok" : "error");
    } else reading.valid = false;
    const uint32_t interval = lowBattery ? LOW_BATTERY_RECHECK_S : settings.intervalS;
    const uint64_t previous = clockState.deadline() ? clockState.deadline() : now;
    clockState.setDeadline(logger::advanceDeadline(previous, clockState.nowUs(), interval));
    lastLive = millis();
    render();
}
void wakeUI(View next, bool fresh) {
    lastActivity = millis();
    view = next;
    if (!display.awake()) display.wake(settings.brightness);
    if (fresh && !service.updating()) {
        battery = power.batteryVoltage();
        reading = sensor.measure();
        lastLive = millis();
    }
    render();
}
void sleepNow() {
    display.sleep();
    // Right button has a physical pull-up. GPIO35 has no internal pull-up.
    rtc_gpio_deinit(static_cast<gpio_num_t>(PIN_BTN_RIGHT));
    uint64_t now = clockState.nowUs();
    uint64_t duration = clockState.deadline() > now ? clockState.deadline() - now : 1000;
    // A held/stuck button must not create an immediate wake loop or keep CPU on.
    if (digitalRead(PIN_BTN_RIGHT) == HIGH)
        esp_sleep_enable_ext0_wakeup(static_cast<gpio_num_t>(PIN_BTN_RIGHT), 0);
    else duration = std::min<uint64_t>(duration, 5000000ULL);
    esp_sleep_enable_timer_wakeup(std::max<uint64_t>(duration, 1000));
    rtc_gpio_init(static_cast<gpio_num_t>(PIN_ADC_ENABLE));
    rtc_gpio_set_direction(static_cast<gpio_num_t>(PIN_ADC_ENABLE), RTC_GPIO_MODE_OUTPUT_ONLY);
    rtc_gpio_set_level(static_cast<gpio_num_t>(PIN_ADC_ENABLE), 1);
    rtc_gpio_hold_en(static_cast<gpio_num_t>(PIN_ADC_ENABLE));
    rtc_gpio_init(static_cast<gpio_num_t>(PIN_TFT_BL));
    rtc_gpio_set_direction(static_cast<gpio_num_t>(PIN_TFT_BL), RTC_GPIO_MODE_OUTPUT_ONLY);
    rtc_gpio_set_level(static_cast<gpio_num_t>(PIN_TFT_BL), 0);
    rtc_gpio_hold_en(static_cast<gpio_num_t>(PIN_TFT_BL));
    Serial.flush();
    clockState.prepareSleep();
    esp_deep_sleep_start();
}
}
void setup() {
    Serial.begin(115200);
    // Keep display dark on every timer boot, before any library initialization.
    rtc_gpio_hold_dis(static_cast<gpio_num_t>(PIN_TFT_BL));
    rtc_gpio_deinit(static_cast<gpio_num_t>(PIN_TFT_BL));
    pinMode(PIN_TFT_BL, OUTPUT);
    digitalWrite(PIN_TFT_BL, LOW);
    ota.begin();
    clockState.begin();
    power.begin();
    rtc_gpio_deinit(static_cast<gpio_num_t>(PIN_BTN_RIGHT));
    pinMode(PIN_BTN_RIGHT, INPUT);
    pinMode(PIN_BTN_LEFT, INPUT_PULLUP);
    right.begin(); left.begin();
    settingsOK = loadSettings(settings);
    appliedInterval = settings.intervalS;
    storage.begin();
    sensor.begin();
    service.setTick(sampleDue);
    if (power.external() && settingsOK) service.begin();
    sampleDue();
    const auto cause = esp_sleep_get_wakeup_cause();
    if (cause == ESP_SLEEP_WAKEUP_UNDEFINED || cause == ESP_SLEEP_WAKEUP_EXT0 || power.external())
        wakeUI(power.external() ? View::Service : View::Values, !reading.valid);
    if (!settingsOK) {
        if (!display.awake()) display.wake(settings.brightness);
        display.message("Settings ERROR", "Inspect serial / NVS backup");
    }
    Serial.printf("T-Display %s usb_sense=%d bench=%d history=%s settings=%s\n", FW_VERSION,
        USB_SENSE_WIRED, FORCE_SERVICE_MODE, storage.ready() ? "ok" : storage.error().c_str(), settingsOK ? "ok" : "error");
}
void loop() {
    if (power.poll()) {
        if (power.external() && settingsOK) {
            service.begin();
            wakeUI(View::Service, true);
        } else if (!service.updating()) {
            service.stop();
            if (display.awake()) wakeUI(View::Values, true);
        }
    }
    service.poll();
    if (!power.external() && service.running() && !service.updating()) service.stop();
    sampleDue();
    if (right.pressed()) {
        if (!display.awake() || view == View::Service) wakeUI(View::Values, true);
        else wakeUI(view == View::Values ? View::Graph : View::Values, view == View::Graph);
    }
    if (left.pressed()) wakeUI(View::Service, false);
    if (display.awake() && !service.updating()) {
        if (view == View::Values && millis() - lastLive >= 5000) {
            reading = sensor.measure();
            lastLive = millis();
            render();
        }
        if (view == View::Service && millis() - lastServiceDraw > 1000) {
            lastServiceDraw = millis(); render();
        }
        if (millis() - lastActivity >= settings.screenTimeoutS * 1000UL) display.sleep();
    }
    if (ota.trial() && settingsOK && storage.ready() && (power.external() ? service.available() : millis() > 1000))
        ota.confirm();
    // An unconfirmed trial stays awake; its watchdog forces a rollback reboot.
    if (!display.awake() && !power.external() && !service.updating() && !ota.trial()) sleepNow();
    delay(5);
}
