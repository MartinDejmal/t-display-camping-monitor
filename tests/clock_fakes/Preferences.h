#pragma once
#include <map>
#include <string>
#include <stdint.h>
extern std::map<std::string,uint32_t> fakeNvs;
class Preferences {
public:
    bool begin(const char*,bool) { return true; }
    uint32_t getUInt(const char *key,uint32_t fallback=0) { auto i=fakeNvs.find(key);return i==fakeNvs.end()?fallback:i->second; }
    size_t putUInt(const char *key,uint32_t value) { fakeNvs[key]=value;return 4; }
    void end() {}
};
