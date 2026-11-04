#include "clock_state.h"
#include <esp_sleep.h>
#include <sys/time.h>
#include <map>
#include <string>
#include <cassert>
#include <cstdio>
int64_t fakeTimer=0,fakeWall=0;
esp_sleep_wakeup_cause_t fakeWake=ESP_SLEEP_WAKEUP_UNDEFINED;
std::map<std::string,uint32_t> fakeNvs;
int main() {
    ClockState clock;clock.begin();
    assert(clock.session()==1 && clock.elapsedS()==0 && clock.epoch()==0);
    clock.setDeadline(60000000);
    fakeTimer=fakeWall=5000000;
    clock.sync(1800000000);
    assert(clock.epoch()==1800000000 && clock.elapsedS()==5 && clock.deadline()==60000000);
    clock.sync(1704067200); // wrong date / backwards correction must not reschedule
    assert(clock.epoch()==1704067200 && clock.elapsedS()==5 && clock.deadline()==60000000);
    clock.clearUtc();
    assert(clock.epoch()==0 && clock.elapsedS()==5 && clock.deadline()==60000000);
    fakeTimer+=2000000;fakeWall+=2000000;
    clock.prepareSleep();
    fakeWake=ESP_SLEEP_WAKEUP_TIMER;fakeTimer=0;fakeWall+=5000000;
    clock.begin();
    assert(clock.session()==1 && clock.elapsedS()==12 && clock.epoch()==0 && clock.deadline()==60000000);
    fakeTimer+=1000000;fakeWall+=1000000;
    clock.prepareSleep();
    fakeWake=ESP_SLEEP_WAKEUP_EXT0;fakeTimer=200000;fakeWall+=1200000;
    clock.begin();
    assert(clock.session()==1 && clock.nowUs()==14200000 && clock.epoch()==0);
    clock.sync(1800000000);
    const uint64_t before=clock.nowUs();
    fakeWall=1900000000LL*1000000;clock.syncSystemTime();
    assert(clock.epoch()==1900000000 && clock.nowUs()==before && clock.deadline()==60000000);
    fakeWake=ESP_SLEEP_WAKEUP_UNDEFINED;fakeTimer=0;clock.begin();
    assert(clock.session()==2 && clock.nowUs()==0 && clock.epoch()==0 && clock.deadline()==0);
    puts("Clock: actual implementation handles missing/wrong UTC, clearing time, backwards/NTP jumps, early deep-sleep wake and new sessions");
}
