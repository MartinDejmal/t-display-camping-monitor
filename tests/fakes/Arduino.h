#pragma once
#include <string>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
class String {
    std::string s_;
public:
    String() = default;
    String(const char *s) : s_(s ? s : "") {}
    String(const std::string &s) : s_(s) {}
    const char *c_str() const { return s_.c_str(); }
    size_t length() const { return s_.length(); }
    bool isEmpty() const { return s_.empty(); }
    bool startsWith(const char *s) const { return s_.find(s) == 0; }
    bool endsWith(const char *s) const { size_t n = strlen(s); return s_.size() >= n && s_.compare(s_.size()-n,n,s)==0; }
    void remove(size_t pos, size_t n) { s_.erase(pos,n); }
    char operator[](size_t i) const { return s_[i]; }
    friend String operator+(const String &a, const String &b) { return a.s_ + b.s_; }
};
inline void yield() {}
