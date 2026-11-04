#pragma once
#include <Arduino.h>
class OtaGuard {
public:
    void begin();
    bool stage();
    bool cancel();
    void confirm();
    bool trial() const { return trial_; }
    const String &error() const { return error_; }
private:
    bool trial_ = false;
    String error_;
};
