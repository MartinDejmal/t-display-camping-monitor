#include <Arduino.h>
#include <TFT_eSPI.h>
#include "version.h"

// Deep sleep duration
static const int      SLEEP_DURATION_SEC = 15;
static const uint64_t SLEEP_DURATION_US  = (uint64_t)SLEEP_DURATION_SEC * 1000000ULL;

// Button 2 (GPIO 35) is used as the deep-sleep wakeup source.
// It is pulled HIGH at rest and goes LOW when pressed.
static const gpio_num_t WAKEUP_PIN = GPIO_NUM_35;

// Backlight pin for the TTGO T-Display
static const int TFT_BL_PIN = 4;

// Display dimensions (landscape, rotation 1)
static const int DISPLAY_W = 240;
static const int DISPLAY_H = 135;

TFT_eSPI tft = TFT_eSPI();

// Timestamp recorded at startup; used to compute the countdown in loop().
static uint32_t startMs = 0;

// Returns a human-readable description of why the device woke up.
static const char *wakeupReason()
{
    switch (esp_sleep_get_wakeup_cause())
    {
    case ESP_SLEEP_WAKEUP_EXT0:
        return "Button press";
    case ESP_SLEEP_WAKEUP_TIMER:
        return "Timer";
    default:
        return "Power-on / reset";
    }
}

// Draws the static version-info screen.
static void drawVersionScreen()
{
    tft.fillScreen(TFT_BLACK);

    // Title
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextFont(4);
    tft.drawString("Camping Monitor", DISPLAY_W / 2, 10);

    // Version
    tft.setTextFont(2);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString("Version: " VERSION_STRING, DISPLAY_W / 2, 45);

    // Wake-up reason
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    char buf[48];
    snprintf(buf, sizeof(buf), "Wake: %s", wakeupReason());
    tft.drawString(buf, DISPLAY_W / 2, 65);

    // Hint for the user
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString("Press BTN2 to wake", DISPLAY_W / 2, 85);
}

// Updates only the countdown line so the rest of the screen is not redrawn.
static void drawCountdown(int secondsLeft)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "Sleep in %2d sec...", secondsLeft);
    tft.setTextDatum(TC_DATUM);
    tft.setTextFont(2);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString(buf, DISPLAY_W / 2, 108);
}

void setup()
{
    Serial.begin(115200);

    // Initialise backlight
    pinMode(TFT_BL_PIN, OUTPUT);
    digitalWrite(TFT_BL_PIN, HIGH);

    // Initialise TFT display
    tft.init();
    tft.setRotation(1);

    drawVersionScreen();

    startMs = millis();

    Serial.printf("Camping Monitor v%s\n", VERSION_STRING);
    Serial.printf("Wake reason: %s\n", wakeupReason());
    Serial.printf("Going to deep sleep in %llu seconds...\n",
                  SLEEP_DURATION_US / 1000000ULL);
}

void loop()
{
    // Compute how many seconds remain before deep sleep.
    uint32_t elapsedSec = (millis() - startMs) / 1000;
    int remaining = SLEEP_DURATION_SEC - (int)elapsedSec;

    if (remaining <= 0)
    {
        Serial.println("Entering deep sleep...");
        Serial.flush();

        // Dim the backlight before sleeping to signal the transition
        digitalWrite(TFT_BL_PIN, LOW);

        // Wake up when Button 2 (GPIO 35) is pressed (active LOW)
        esp_sleep_enable_ext0_wakeup(WAKEUP_PIN, 0);

        esp_deep_sleep_start();
        // Execution never reaches here after deep sleep
    }

    // Redraw the countdown only when the displayed second changes.
    static int lastRemaining = -1;
    if (remaining != lastRemaining)
    {
        drawCountdown(remaining);
        lastRemaining = remaining;
    }

    delay(100);
}
