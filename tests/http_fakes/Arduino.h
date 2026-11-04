#pragma once
#include "../fakes/Arduino.h"
extern uint32_t httpFakeMs;
using PGM_P = const char *;
inline uint32_t millis() { return httpFakeMs; }
inline void delay(uint32_t ms) { httpFakeMs += ms; }
