#pragma once
#include <string>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <cctype>
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
    bool startsWith(const String &s) const { return startsWith(s.c_str()); }
    bool endsWith(const char *s) const { size_t n = strlen(s); return s_.size() >= n && s_.compare(s_.size()-n,n,s)==0; }
    void remove(size_t pos, size_t n) { s_.erase(pos,n); }
    void remove(size_t pos) { s_.erase(pos); }
    int indexOf(const String &s, size_t pos = 0) const { auto n=s_.find(s.s_,pos); return n==std::string::npos ? -1 : static_cast<int>(n); }
    int indexOf(char c, size_t pos = 0) const { auto n=s_.find(c,pos); return n==std::string::npos ? -1 : static_cast<int>(n); }
    int lastIndexOf(char c) const { auto n=s_.rfind(c); return n==std::string::npos ? -1 : static_cast<int>(n); }
    String substring(size_t pos, size_t end = std::string::npos) const { pos=std::min(pos,s_.size()); return s_.substr(pos,std::min(end,s_.size())-pos); }
    void trim() { auto a=s_.find_first_not_of(" \t\r\n"),b=s_.find_last_not_of(" \t\r\n"); s_=a==std::string::npos ? "" : s_.substr(a,b-a+1); }
    bool equalsIgnoreCase(const String &s) const { return s_.size()==s.s_.size() && std::equal(s_.begin(),s_.end(),s.s_.begin(),[](char a,char b){return std::tolower(static_cast<unsigned char>(a))==std::tolower(static_cast<unsigned char>(b));}); }
    String &operator+=(char c) { s_+=c; return *this; }
    String &operator+=(const String &s) { s_+=s.s_; return *this; }
    bool concat(const char *s) { s_+=s; return true; }
    std::string::const_iterator begin() const { return s_.begin(); }
    std::string::const_iterator end() const { return s_.end(); }
    char operator[](size_t i) const { return s_[i]; }
    friend String operator+(const String &a, const String &b) { return a.s_ + b.s_; }
    friend bool operator==(const String &a,const String &b) { return a.s_==b.s_; }
    friend bool operator!=(const String &a,const String &b) { return !(a==b); }
};
inline void yield() {}
