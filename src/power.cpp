#include "power.h"
#include "pins.h"
#include "config.h"
#include <driver/rtc_io.h>
#include <math.h>
bool Power::sense() {
    if (FORCE_SERVICE_MODE) return true;
    if (!USB_SENSE_WIRED) return false;
    uint32_t mv = 0;
    for (int i = 0; i < 4; ++i) mv += analogReadMilliVolts(PIN_USB_SENSE);
    // Hysteresis, expressed at the midpoint of a 1:2 VBUS divider.
    return mv / 4 > (external_ ? 1500 : 1900);
}
void Power::begin() {
    rtc_gpio_hold_dis(static_cast<gpio_num_t>(PIN_ADC_ENABLE));
    rtc_gpio_deinit(static_cast<gpio_num_t>(PIN_ADC_ENABLE));
    pinMode(PIN_ADC_ENABLE, OUTPUT);
    digitalWrite(PIN_ADC_ENABLE, HIGH);
    analogSetAttenuation(ADC_11db);
    analogReadResolution(12);
    pinMode(PIN_USB_SENSE, USB_SENSE_WIRED ? INPUT : INPUT_PULLDOWN);
    delay(3);
    external_ = candidate_ = sense();
}
bool Power::poll() {
    if (millis() - polled_ < 100) return false;
    polled_ = millis();
    bool next = sense();
    if (next != candidate_) { candidate_ = next; changed_ = millis(); }
    if (candidate_ != external_ && millis() - changed_ >= 500) {
        external_ = candidate_;
        return true;
    }
    return false;
}
float Power::batteryVoltage() {
    if (external_) return NAN;
    uint32_t mv = 0;
    for (int i = 0; i < 8; ++i) mv += analogReadMilliVolts(PIN_BAT_ADC);
    const float voltage = mv * 2.0f / 8000.0f;
    return voltage >= 2.0f && voltage <= 4.35f ? voltage : NAN;
}
