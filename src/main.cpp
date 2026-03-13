/*
 * Camping Monitor – firmware for TTGO T-Display (ESP32 + ST7789V 1.14" IPS)
 *
 * Monitors:
 *   • Internal LiPo battery voltage (GPIO 34, on-board 1:2 voltage divider)
 *   • External 12 V battery / solar voltage (GPIO 32, user-supplied divider)
 *   • Ambient temperature & humidity (DHT22 on GPIO 27)
 *
 * Controls:
 *   • Left button  (GPIO 0)  – cycle screens
 *   • Right button (GPIO 35) – toggle backlight / wake from dim
 *
 * Power saving:
 *   • Backlight dims after BACKLIGHT_TIMEOUT_MS of inactivity
 *   • Deep-sleep entered after one full display cycle (configurable)
 *   • Wake sources: timer (SLEEP_INTERVAL_US) or either button
 */

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <DHT.h>
#include <driver/adc.h>
#include <driver/gpio.h>
#include "pins.h"
#include "config.h"

// ─────────────────────────────────────────────────────────────
//  Globals
// ─────────────────────────────────────────────────────────────
TFT_eSPI tft = TFT_eSPI();
DHT dht(PIN_DHT, DHT_TYPE);

struct SensorData {
    float vBat;          // Internal LiPo voltage (V)
    int   batPct;        // LiPo charge percentage (0-100)
    float vExt;          // External battery/solar voltage (V)
    float tempC;         // Temperature (°C)
    float humidity;      // Relative humidity (%)
    bool  dhtOk;         // DHT22 read succeeded
};

static SensorData  g_data         = {};
static uint8_t     g_screen       = SCREEN_OVERVIEW;
static uint32_t    g_lastActivity = 0;
static bool        g_backlightOn  = true;
static uint32_t    g_lastCycle    = 0;  // Reset each boot (incl. wake from deep sleep)

// ─────────────────────────────────────────────────────────────
//  Helper: multi-sample ADC average
// ─────────────────────────────────────────────────────────────
static int adcAverage(uint8_t pin, uint8_t samples)
{
    long sum = 0;
    for (uint8_t i = 0; i < samples; ++i) {
        sum += analogRead(pin);
        delay(2);
    }
    return (int)(sum / samples);
}

// ─────────────────────────────────────────────────────────────
//  Helper: clamp percentage to 0-100
// ─────────────────────────────────────────────────────────────
static int voltageToPercent(float v, float vMax, float vMin)
{
    if (v >= vMax) return 100;
    if (v <= vMin) return 0;
    return (int)(100.0f * (v - vMin) / (vMax - vMin));
}

// ─────────────────────────────────────────────────────────────
//  Read all sensors
// ─────────────────────────────────────────────────────────────
static void readSensors(SensorData &d)
{
    // Internal LiPo
    int rawBat = adcAverage(PIN_BAT_ADC, ADC_SAMPLES);
    d.vBat  = rawBat * BAT_ADC_SCALE;
    d.batPct = voltageToPercent(d.vBat, LIPO_VOLT_MAX, LIPO_VOLT_MIN);

    // External battery / solar (voltage divider on GPIO32)
    int rawExt = adcAverage(PIN_EXT_ADC, ADC_SAMPLES);
    d.vExt = rawExt * VEXT_ADC_SCALE;

    // DHT22
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    d.dhtOk = !(isnan(h) || isnan(t));
    if (d.dhtOk) {
        d.tempC    = t;
        d.humidity = h;
    }
}

// ─────────────────────────────────────────────────────────────
//  Backlight helpers
// ─────────────────────────────────────────────────────────────
static void setBacklight(uint8_t brightness)
{
    analogWrite(PIN_TFT_BL, brightness);
}

static void wakeBacklight()
{
    setBacklight(BACKLIGHT_DEFAULT);
    g_backlightOn  = true;
    g_lastActivity = millis();
}

// ─────────────────────────────────────────────────────────────
//  Draw helpers
// ─────────────────────────────────────────────────────────────
static void drawHeader(const char *title)
{
    tft.fillRect(0, 0, tft.width(), 20, TFT_DARKGREY);
    tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(1);
    tft.drawString(title, tft.width() / 2, 10);
}

static void drawFooter()
{
    // Page indicator dots
    int dotY  = tft.height() - 8;
    int dotSpacing = 12;
    int startX = tft.width() / 2 - ((SCREEN_COUNT - 1) * dotSpacing) / 2;
    for (int i = 0; i < SCREEN_COUNT; ++i) {
        uint16_t color = (i == g_screen) ? TFT_WHITE : TFT_DARKGREY;
        tft.fillCircle(startX + i * dotSpacing, dotY, 3, color);
    }
}

// Battery icon width=30, height=14
static void drawBatteryIcon(int x, int y, int pct, uint16_t fillColor)
{
    // Outer rectangle
    tft.drawRect(x, y, 28, 14, TFT_WHITE);
    tft.fillRect(x + 28, y + 4, 3, 6, TFT_WHITE); // nub
    // Fill
    int fillW = (int)(26.0f * pct / 100.0f);
    if (fillW > 0) {
        tft.fillRect(x + 1, y + 1, fillW, 12, fillColor);
    }
}

// ─────────────────────────────────────────────────────────────
//  Screen renderers
// ─────────────────────────────────────────────────────────────
static void drawScreenOverview(const SensorData &d)
{
    tft.fillScreen(TFT_BLACK);
    drawHeader("Camping Monitor");

    // -- Internal LiPo row --
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setTextDatum(ML_DATUM);
    tft.setTextSize(1);
    tft.drawString("LiPo:", 4, 38);

    uint16_t batColor = d.batPct > 40 ? TFT_GREEN : (d.batPct > 20 ? TFT_YELLOW : TFT_RED);
    drawBatteryIcon(50, 31, d.batPct, batColor);

    tft.setTextColor(batColor, TFT_BLACK);
    char buf[24];
    snprintf(buf, sizeof(buf), "%.2f V %d %%", d.vBat, d.batPct);
    tft.drawString(buf, 86, 38);

    // -- External battery row --
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("Ext:", 4, 65);

    int extPct = voltageToPercent(d.vExt, EXT_VOLT_MAX, EXT_VOLT_MIN);
    uint16_t extColor = extPct > 40 ? TFT_GREEN : (extPct > 20 ? TFT_YELLOW : TFT_RED);
    drawBatteryIcon(50, 58, extPct, extColor);

    snprintf(buf, sizeof(buf), "%.2f V", d.vExt);
    tft.setTextColor(extColor, TFT_BLACK);
    tft.drawString(buf, 86, 65);

    // -- Temperature row --
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("Temp:", 4, 92);
    if (d.dhtOk) {
        snprintf(buf, sizeof(buf), "%.1f C  %.0f %%", d.tempC, d.humidity);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
    } else {
        snprintf(buf, sizeof(buf), "No sensor");
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    }
    tft.drawString(buf, 50, 92);

    drawFooter();
}

static void drawScreenBattery(const SensorData &d)
{
    tft.fillScreen(TFT_BLACK);
    drawHeader("Battery");

    int cx = tft.width() / 2;

    // LiPo section
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(1);
    tft.drawString("Internal LiPo", cx, 32);

    uint16_t batColor = d.batPct > 40 ? TFT_GREEN : (d.batPct > 20 ? TFT_YELLOW : TFT_RED);
    char buf[24];
    snprintf(buf, sizeof(buf), "%.2f V", d.vBat);
    tft.setTextColor(batColor, TFT_BLACK);
    tft.setTextSize(2);
    tft.drawString(buf, cx, 52);

    tft.setTextSize(1);
    snprintf(buf, sizeof(buf), "%d%%", d.batPct);
    tft.drawString(buf, cx, 70);

    // Separator
    tft.drawFastHLine(10, 82, tft.width() - 20, TFT_DARKGREY);

    // External battery section
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setTextSize(1);
    tft.drawString("External / Solar", cx, 94);

    int extPct = voltageToPercent(d.vExt, EXT_VOLT_MAX, EXT_VOLT_MIN);
    uint16_t extColor = extPct > 40 ? TFT_GREEN : (extPct > 20 ? TFT_YELLOW : TFT_RED);
    snprintf(buf, sizeof(buf), "%.2f V", d.vExt);
    tft.setTextColor(extColor, TFT_BLACK);
    tft.setTextSize(2);
    tft.drawString(buf, cx, 114);

    tft.setTextSize(1);
    snprintf(buf, sizeof(buf), "%d%%", extPct);
    tft.drawString(buf, cx, 132);

    drawFooter();
}

static void drawScreenEnvironment(const SensorData &d)
{
    tft.fillScreen(TFT_BLACK);
    drawHeader("Environment");

    int cx = tft.width() / 2;
    tft.setTextDatum(MC_DATUM);

    if (!d.dhtOk) {
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.setTextSize(1);
        tft.drawString("DHT22 not found", cx, tft.height() / 2 - 10);
        tft.drawString("Check GPIO 27", cx, tft.height() / 2 + 10);
    } else {
        char buf[24];

        // Temperature
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.setTextSize(1);
        tft.drawString("Temperature", cx, 32);

        uint16_t tempColor = (d.tempC < 10) ? TFT_CYAN
                           : (d.tempC > 35)  ? TFT_RED
                                              : TFT_WHITE;
        tft.setTextColor(tempColor, TFT_BLACK);
        tft.setTextSize(2);
        snprintf(buf, sizeof(buf), "%.1f C", d.tempC);
        tft.drawString(buf, cx, 52);

        // Separator
        tft.drawFastHLine(10, 78, tft.width() - 20, TFT_DARKGREY);

        // Humidity
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.setTextSize(1);
        tft.drawString("Humidity", cx, 90);

        uint16_t humColor = d.humidity > 70 ? TFT_YELLOW : TFT_WHITE;
        tft.setTextColor(humColor, TFT_BLACK);
        tft.setTextSize(2);
        snprintf(buf, sizeof(buf), "%.0f %%", d.humidity);
        tft.drawString(buf, cx, 110);
    }

    drawFooter();
}

static void drawCurrentScreen()
{
    switch (g_screen) {
        case SCREEN_OVERVIEW:    drawScreenOverview(g_data);    break;
        case SCREEN_BATTERY:     drawScreenBattery(g_data);     break;
        case SCREEN_ENVIRONMENT: drawScreenEnvironment(g_data); break;
        default:                 drawScreenOverview(g_data);    break;
    }
}

// ─────────────────────────────────────────────────────────────
//  Button ISR wrappers (IRAM-safe, minimal work)
// ─────────────────────────────────────────────────────────────
static volatile bool g_btnLeftPressed  = false;
static volatile bool g_btnRightPressed = false;

static void IRAM_ATTR onBtnLeft()  { g_btnLeftPressed  = true; }
static void IRAM_ATTR onBtnRight() { g_btnRightPressed = true; }

// ─────────────────────────────────────────────────────────────
//  Enter deep sleep
// ─────────────────────────────────────────────────────────────
static void enterDeepSleep()
{
    setBacklight(0);
    tft.fillScreen(TFT_BLACK);

    // Wake on right button press (GPIO 35 pulled LOW) or timer.
    // ext0 is used here for maximum ESP-IDF v4/v5 compatibility;
    // only one GPIO can be registered with ext0 at a time.
    esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_BTN_RIGHT, 0);
    esp_sleep_enable_timer_wakeup(SLEEP_INTERVAL_US);

    esp_deep_sleep_start();
}

// ─────────────────────────────────────────────────────────────
//  setup()
// ─────────────────────────────────────────────────────────────
void setup()
{
    Serial.begin(115200);

    // ADC configuration: use 11 dB attenuation for full 0-3.3 V range
    analogSetAttenuation(ADC_11db);
    analogReadResolution(12);

    // DHT22
    dht.begin();

    // Display
    tft.init();
    tft.setRotation(TFT_ROTATION);
    tft.fillScreen(TFT_BLACK);

    // Backlight (PWM)
    pinMode(PIN_TFT_BL, OUTPUT);
    setBacklight(BACKLIGHT_DEFAULT);
    g_lastActivity = millis();

    // Buttons (internal pull-up, active LOW)
    pinMode(PIN_BTN_LEFT,  INPUT_PULLUP);
    pinMode(PIN_BTN_RIGHT, INPUT_PULLUP);
    attachInterrupt(PIN_BTN_LEFT,  onBtnLeft,  FALLING);
    attachInterrupt(PIN_BTN_RIGHT, onBtnRight, FALLING);

    // Splash screen
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(1);
    tft.drawString("Camping Monitor", tft.width() / 2, tft.height() / 2 - 10);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString("Reading sensors...", tft.width() / 2, tft.height() / 2 + 10);

    // Initial sensor read (DHT22 needs ~2 s after power-on)
    delay(2000);
    g_lastCycle = millis();  // Align cycle timer with end of setup
    readSensors(g_data);
    drawCurrentScreen();
}

// ─────────────────────────────────────────────────────────────
//  loop()
// ─────────────────────────────────────────────────────────────
void loop()
{
    uint32_t now = millis();

    // --- Handle left button: cycle screens ---
    if (g_btnLeftPressed) {
        g_btnLeftPressed = false;
        if (!g_backlightOn) {
            wakeBacklight();
        } else {
            g_screen = (g_screen + 1) % SCREEN_COUNT;
            wakeBacklight();
            readSensors(g_data);
            drawCurrentScreen();
        }
    }

    // --- Handle right button: toggle backlight ---
    if (g_btnRightPressed) {
        g_btnRightPressed = false;
        if (!g_backlightOn) {
            wakeBacklight();
        } else {
            setBacklight(BACKLIGHT_DIM);
            g_backlightOn  = false;
        }
    }

    // --- Backlight timeout ---
    if (g_backlightOn && (now - g_lastActivity > BACKLIGHT_TIMEOUT_MS)) {
        setBacklight(BACKLIGHT_DIM);
        g_backlightOn = false;
    }

    // --- Periodic sensor refresh + screen auto-cycle ---
    if (now - g_lastCycle > SCREEN_CYCLE_MS) {
        g_lastCycle = now;
        if (g_backlightOn) {
            readSensors(g_data);
            g_screen = (g_screen + 1) % SCREEN_COUNT;
            drawCurrentScreen();
        }
    }

    // Small delay to avoid busy-polling
    delay(50);
}
