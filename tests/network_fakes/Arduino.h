#pragma once
#include "../fakes/Arduino.h"
extern uint32_t networkMs;
inline uint32_t millis(){return networkMs;}
void delay(uint32_t ms);
