#include "network.h"
#include <WiFi.h>
#include <esp_sntp.h>
#include <cassert>
#include <iostream>
uint32_t networkMs=0,connectionAt=0,ntpAt=0,syncedEpoch=0;
uint32_t connectionDelay=250,ntpReply=1800000000;
String activeSsid,goodSsid;
bool networkOutage=false,ntpRunning=false,ntpSuccess=true;
float voltage=3.68f;
uint32_t usbAt=UINT32_MAX,lowAt=UINT32_MAX;
std::vector<std::string> networkAttempts;
FakeWifi WiFi;
void (*notify)(timeval*)=nullptr;
void sntp_stop(){ntpRunning=false;}
void sntp_set_time_sync_notification_cb(void (*cb)(timeval*)){notify=cb;}
void configTime(long,int,const char*,const char*){ntpRunning=true;ntpAt=networkMs;}
void delay(uint32_t ms){networkMs+=ms;if(ntpRunning && ntpSuccess && networkMs-ntpAt>=1000 && notify){timeval t{static_cast<long>(ntpReply),0};notify(&t);ntpRunning=false;}}
void ClockState::sync(uint32_t epoch){syncedEpoch=epoch;}
bool Power::poll(){if(networkMs>=usbAt)external_=true;return external_;}
float Power::batteryVoltage(){return networkMs>=lowAt?3.3f:voltage;}
void reset(){networkMs=connectionAt=0;networkAttempts.clear();activeSsid="";goodSsid="";networkOutage=false;ntpRunning=false;ntpSuccess=true;voltage=3.68f;usbAt=lowAt=UINT32_MAX;syncedEpoch=0;connectionDelay=250;ntpReply=1800000000;}
int main(){
    Settings s;s.wifi[0].ssid="missing";s.wifi[1].ssid="hidden";s.wifi[1].hidden=true;s.wifi[1].password="secret-test";
    reset();goodSsid="hidden";WifiConnector connector(s);connector.begin();
    assert(networkAttempts==std::vector<std::string>{"missing"});
    networkMs=14999;connector.poll();assert(networkAttempts.size()==1);
    networkMs=15000;connector.poll();assert(networkAttempts==std::vector<std::string>({"missing","hidden"}));
    networkMs=15300;connector.poll();assert(connector.index()==1 && !connector.exhausted());
    networkOutage=true;connector.poll();assert(networkAttempts.back()=="missing");
    connector.stop();assert(WiFi.modeValue==WIFI_OFF);
    reset();Settings five;for(size_t i=0;i<5;++i)five.wifi[i].ssid=std::string("network")+std::to_string(i);
    ClockState clock;Power power;
    assert(syncBatteryTime(five,clock,power)==BatteryTimeResult::NoNetwork);
    assert(networkMs==75000 && networkAttempts.size()==5 && WiFi.modeValue==WIFI_OFF && !notify);
    reset();goodSsid="hidden";Power second;
    assert(syncBatteryTime(s,clock,second)==BatteryTimeResult::Synced);
    assert(syncedEpoch==1800000000 && networkMs<17000 && WiFi.modeValue==WIFI_OFF && !notify);
    reset();goodSsid="hidden";ntpSuccess=false;Power third;
    assert(syncBatteryTime(s,clock,third)==BatteryTimeResult::NtpTimeout);
    assert(networkMs<24000 && !syncedEpoch && WiFi.modeValue==WIFI_OFF);
    reset();goodSsid="network4";connectionDelay=14900;ntpSuccess=false;Power worst;
    assert(syncBatteryTime(five,clock,worst)==BatteryTimeResult::NtpTimeout);
    assert(networkMs==82900 && networkAttempts.size()==5 && WiFi.modeValue==WIFI_OFF);
    reset();goodSsid="hidden";ntpReply=0;Power invalidTime;
    assert(syncBatteryTime(s,clock,invalidTime)==BatteryTimeResult::NtpTimeout && !syncedEpoch);
    reset();voltage=3.49f;Power low;
    assert(syncBatteryTime(s,clock,low)==BatteryTimeResult::LowBattery && networkAttempts.empty());
    reset();lowAt=2000;Power dropping;
    assert(syncBatteryTime(s,clock,dropping)==BatteryTimeResult::LowBattery && networkMs==2000 && WiFi.modeValue==WIFI_OFF);
    reset();usbAt=500;Power inserted;
    assert(syncBatteryTime(s,clock,inserted)==BatteryTimeResult::UsbConnected && networkMs==500 && WiFi.modeValue==WIFI_OFF);
    reset();s.autoTime=false;Power disabled;
    assert(syncBatteryTime(s,clock,disabled)==BatteryTimeResult::Skipped && networkAttempts.empty());
    reset();
    Settings noNetworks;WifiConnector none(noNetworks);none.begin();assert(none.exhausted() && networkAttempts.empty());
    reset();WifiConnector retry(five);retry.begin();for(int i=0;i<5;++i){networkMs+=15000;retry.poll();}
    assert(retry.exhausted());networkMs+=60000;retry.poll();assert(networkAttempts.size()==6 && networkAttempts.back()=="network0");
    reset();goodSsid="hidden";connectionDelay=16000;WifiConnector dhcp(s);dhcp.begin();
    networkMs=15000;dhcp.poll();networkMs=30000;dhcp.poll();
    assert(dhcp.exhausted() && activeSsid=="hidden"); // retain last pending lease on USB
    networkMs=31000;dhcp.poll();assert(WiFi.status()==WL_CONNECTED && networkAttempts.size()==2);
    reset();networkMs=UINT32_MAX-4000;WifiConnector wrap(s);wrap.begin();networkMs+=15000;wrap.poll();assert(networkAttempts.size()==2);
    std::cout<<"Network: ordered five-profile failover, directed hidden SSID, reconnect, retry, wraparound, bounded NTP, low battery and USB interruption OK\n";
}
