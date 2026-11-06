#pragma once
#include "Arduino.h"
#include <map>
extern std::map<std::string,std::string> settingsNvs;
extern bool settingsWriteFails;
class Preferences {
    std::string prefix_;
public:
    bool begin(const char *name,bool){prefix_=std::string(name)+"/";return true;}
    String getString(const char *key,const char *fallback){auto i=settingsNvs.find(prefix_+key);return i==settingsNvs.end()?String(fallback):String(i->second);}
    size_t putString(const char *key,const String &value){if(settingsWriteFails)return 0;settingsNvs[prefix_+key]=value.c_str();return value.length();}
    void end(){}
};
