#pragma once
#include <WebServer.h>
#include <functional>
#include "config.h"
#include "clock_state.h"
#include "storage.h"
#include "sensor.h"
#include "power.h"
#include "ota_guard.h"
class Service {
public:
    Service(Settings &settings, ClockState &clock, Storage &storage,
            Power &power, OtaGuard &ota, Reading &reading);
    void begin();
    void stop();
    void poll();
    bool running() const { return running_; }
    bool updating() const { return uploading_ || rebootAt_ != 0; }
    bool available() const;
    String network() const;
    String ip() const;
    void setTick(std::function<void()> tick) { tick_ = tick; }
private:
    Settings &settings_;
    ClockState &clock_;
    Storage &storage_;
    Power &power_;
    OtaGuard &ota_;
    Reading &reading_;
    WebServer server_{80};
    bool running_ = false, ap_ = false, connected_ = false, routes_ = false;
    bool ntpStarted_ = false;
    bool uploading_ = false, uploadAuthorized_ = false, uploadOK_ = false;
    bool familySeen_ = false;
    size_t familyMatch_ = 0;
    uint32_t started_ = 0, rebootAt_ = 0;
    String name_, csrf_, uploadError_;
    std::function<void()> tick_;
    bool authorize(bool write = false);
    void routes();
    void startAP();
    void status();
    void sessions();
    void page(const char *id, const char *title, const char *body);
    bool historyReference(uint32_t &session, uint32_t &elapsed);
    void history();
    void csv();
    void configure();
    void upload();
    void uploadFinished();
    void error(int code, const char *message);
};
