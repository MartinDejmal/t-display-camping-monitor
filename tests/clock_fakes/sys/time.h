#pragma once
#include_next <sys/time.h>
#include <time.h>
#include <stdint.h>
extern int64_t fakeWall;
inline int fakeClockGetTime(timeval *tv,void*) { tv->tv_sec=fakeWall/1000000;tv->tv_usec=fakeWall%1000000;return 0; }
inline int fakeClockSetTime(const timeval *tv,const void*) { fakeWall=static_cast<int64_t>(tv->tv_sec)*1000000+tv->tv_usec;return 0; }
inline time_t fakeClockTime(time_t *out) { time_t value=fakeWall/1000000;if(out)*out=value;return value; }
#define gettimeofday fakeClockGetTime
#define settimeofday fakeClockSetTime
#define time fakeClockTime
