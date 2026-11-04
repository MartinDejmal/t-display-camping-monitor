#include "record.h"
#include "ota_marker.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <algorithm>
using namespace logger;
int main() {
    const char *golden = "123456789";
    assert(crc16(reinterpret_cast<const uint8_t *>(golden), 9) == 0x29b1);
    Record r{};
    r.session = 42;
    r.elapsedS = 3600;
    r.epoch = 1800000000;
    r.temperature = -1234;
    r.humidity = 10000;
    r.pressure = 100100;
    r.flags = FLAG_SENSOR_OK | FLAG_UTC;
    seal(r);
    assert(valid(r));
    for (size_t i = 0; i < sizeof(r); ++i) {
        Record changed = r;
        reinterpret_cast<uint8_t *>(&changed)[i] ^= 1;
        assert(!valid(changed));
    }
    // An intact record for another schema must also be rejected.
    Record other = r;
    other.flags = 0x2003;
    other.crc = crc16(reinterpret_cast<uint8_t *>(&other), offsetof(Record, crc));
    assert(!valid(other));
    assert(r.temperature == -1234 && sizeof(r) == 24);
    assert(advanceDeadline(60000000ULL, 10000000ULL, 60) == 60000000ULL);
    assert(advanceDeadline(60000000ULL, 60000000ULL, 60) == 120000000ULL);
    assert(advanceDeadline(60000000ULL, 241000000ULL, 60) == 300000000ULL);
    assert(advanceDeadline(0, 1500000, 60) == 60000000ULL);
    assert(advanceDeadline(60000000ULL, 241000000ULL, 0) == 60000000ULL);
    // Early button wake: use actual 5 s sleep, not the armed 60 s timer.
    assert(resumeElapsed(15000000ULL, 100000000LL, 105000000LL) == 20000000ULL);
    assert(resumeElapsed(15000000ULL, 100000000LL, 160000000LL) == 75000000ULL);
    assert(resumeElapsed(15000000ULL, 100000000LL, 90000000LL) == 15000000ULL);
    assert(resumeElapsed(5000000000ULL, 100000000LL, 160000000LL) == 5060000000ULL);
    uint32_t age = 0;
    assert(ageSeconds(r, 42, 3660, 0, age) && age == 60);
    assert(ageSeconds(r, 43, 60, 1800000120, age) && age == 120);
    assert(!ageSeconds(r, 43, 60, 0, age));
    r.flags &= ~FLAG_UTC;
    assert(!ageSeconds(r, 43, 60, 1800000120, age));
    assert(!ageSeconds(r, 42, 3599, 0, age));
    const char *marker = "|TDLOGGER:ORIGINAL:USB=1:BENCH=0:RECORD=1:PART=1|";
    std::string payload = std::string("garbage|TDLOGGx") + marker + "trailer";
    for (size_t chunk = 1; chunk <= payload.size(); ++chunk) {
        size_t matched=0; bool found=false;
        for (size_t offset=0; offset<payload.size(); offset+=chunk)
            feedOtaMarker(reinterpret_cast<const uint8_t*>(payload.data()+offset),
                std::min(chunk,payload.size()-offset),marker,matched,found);
        assert(found);
    }
    size_t matched=0; bool found=false;
    std::string wrong = "|TDLOGGER:ORIGINAL:USB=0:BENCH=1:RECORD=1:PART=1|";
    feedOtaMarker(reinterpret_cast<const uint8_t*>(wrong.data()),wrong.size(),marker,matched,found);
    assert(!found);
    std::cout << "Core: CRC, schema, deadlines, early wake, time gaps, OTA marker/chunk boundaries OK\n";
}
