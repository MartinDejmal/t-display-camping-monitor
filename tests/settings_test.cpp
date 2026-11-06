#include "settings.h"
#include <cassert>
#include <iostream>
#include <map>
std::map<std::string,std::string> settingsNvs;
bool settingsWriteFails=false;
int main(){
    const std::string legacy=R"({"version":1,"interval_s":120,"screen_timeout_s":15,"brightness":100,"ssid":"old-network","wifi_password":"old-test-secret","admin_password":"admin-test-secret","auth_enabled":false,"auto_time":true})";
    settingsNvs["settings/json"]=legacy;
    Settings s;assert(loadSettings(s));assert(s.wifi[0].ssid=="old-network" && s.wifi[0].password=="old-test-secret");
    assert(!s.authEnabled && s.autoTime && s.intervalS==120);
    assert(settingsNvs["settings/json"]==legacy); // no persistent migration on trial boot
    for(size_t i=1;i<5;++i)assert(s.wifi[i].ssid.isEmpty() && s.wifi[i].password.isEmpty());
    DynamicJsonDocument doc(6144);
    auto update=[&](const char *json,Settings &target){doc.clear();assert(!deserializeJson(doc,json));return updateWifiProfiles(target,s,doc.as<JsonArrayConst>());};
    Settings reordered=s;
    assert(update(R"([{"ssid":"new-network","password":"new-test-secret","hidden":true},{"ssid":"old-network"},{"ssid":"open-network","open":true},{"ssid":""},{"ssid":""}])",reordered));
    assert(reordered.wifi[0].hidden && reordered.wifi[1].password==s.wifi[0].password && reordered.wifi[2].password.isEmpty());
    assert(s.wifi[0].ssid=="old-network");
    for(const char *bad:{
        R"([{"ssid":"unknown"},{"ssid":""},{"ssid":""},{"ssid":""},{"ssid":""}])",
        R"([{"ssid":"old-network"},{"ssid":"old-network"},{"ssid":""},{"ssid":""},{"ssid":""}])",
        R"([{"ssid":"old-network","hidden":"false"},{"ssid":""},{"ssid":""},{"ssid":""},{"ssid":""}])",
        R"([{"ssid":"new","password":"1234567"},{"ssid":""},{"ssid":""},{"ssid":""},{"ssid":""}])",
        R"([{"ssid":"new","password":"long-test-secret","open":true},{"ssid":""},{"ssid":""},{"ssid":""},{"ssid":""}])",
        R"([{"ssid":"old-network"}])"
    }){Settings badTarget=s;assert(!update(bad,badTarget));assert(settingsNvs["settings/json"]==legacy);}
    assert(saveSettings(reordered));Settings loaded;assert(loadSettings(loaded));
    assert(loaded.wifi[0].hidden && loaded.wifi[1].password=="old-test-secret" && !loaded.authEnabled);
    for(size_t i=0;i<5;++i){loaded.wifi[i].ssid=std::string(31,'s')+char('0'+i);loaded.wifi[i].password=std::string(63,'\\');loaded.wifi[i].hidden=true;}
    assert(saveSettings(loaded));assert(settingsNvs["settings/json"].size()<4096);
    Settings roundtrip;assert(loadSettings(roundtrip));assert(roundtrip.wifi[4].password==loaded.wifi[4].password);
    const auto before=settingsNvs;settingsWriteFails=true;assert(!saveSettings(roundtrip) && settingsNvs==before);settingsWriteFails=false;
    settingsNvs["settings/json"]="{broken";assert(!loadSettings(roundtrip));assert(settingsNvs["settings/json"]=="{broken");
    settingsNvs.clear();Settings fresh;assert(loadSettings(fresh) && fresh.adminPassword.length()==16 && fresh.wifi[0].ssid.isEmpty());
    std::cout<<"Settings: actual JSON/NVS migration, five profiles, hidden/open networks, secret preservation by SSID, validation and atomic failed save OK\n";
}
