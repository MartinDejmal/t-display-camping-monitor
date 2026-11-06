#pragma once
#include "Arduino.h"
#include <vector>
enum {WIFI_OFF,WIFI_STA,WL_IDLE_STATUS=0,WL_CONNECTED=3};
extern String activeSsid, goodSsid;
extern uint32_t connectionAt;
extern uint32_t connectionDelay;
extern bool networkOutage;
extern std::vector<std::string> networkAttempts;
class FakeWifi {
public:
    int modeValue=WIFI_OFF;
    void persistent(bool){}
    void mode(int value){modeValue=value;}
    void setSleep(bool){}
    void setAutoReconnect(bool){}
    void setHostname(const char*){}
    void disconnect(bool=false,bool=false){activeSsid="";}
    void begin(const char *ssid,const char*){activeSsid=ssid;connectionAt=networkMs;networkAttempts.push_back(ssid);}
    int status(){return !networkOutage && !goodSsid.isEmpty() && activeSsid==goodSsid && networkMs-connectionAt>=connectionDelay ? WL_CONNECTED : WL_IDLE_STATUS;}
    void scanDelete(){}
};
extern FakeWifi WiFi;
void configTime(long,int,const char*,const char*);
