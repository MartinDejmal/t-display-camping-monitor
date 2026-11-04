#pragma once
#include "Arduino.h"
#include <dirent.h>
#include <sys/stat.h>
#include <memory>
#include <vector>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
constexpr const char *FILE_READ = "rb";
constexpr const char *FILE_APPEND = "ab";
extern std::string fakeRoot;
extern size_t fakeCapacity, fakeWriteLimit;
extern bool fakeMount, fakeErased;
extern unsigned fakeFormats;
inline std::vector<std::string> fakeNames() {
    std::vector<std::string> names;
    DIR *dir = opendir(fakeRoot.c_str());
    if (!dir) return names;
    while (auto *entry = readdir(dir)) if (entry->d_name[0] == 'b') names.push_back(entry->d_name);
    closedir(dir); return names;
}
class File {
    struct State {
        FILE *file = nullptr;
        std::string name;
        std::vector<std::string> entries;
        size_t index = 0;
        bool directory = false, failed = false;
        ~State() { if(file) fclose(file); }
    };
    std::shared_ptr<State> s_;
public:
    File() = default;
    File(const std::string &name, const char *mode) : s_(std::make_shared<State>()) {
        s_->name = name;
        if(name == "/") { s_->directory=true; s_->entries=fakeNames(); }
        else s_->file=fopen((fakeRoot+name).c_str(),mode);
    }
    explicit operator bool() const { return s_ && (s_->directory || s_->file); }
    String name() const { return s_ ? s_->name : ""; }
    File openNextFile() {
        if (!s_ || s_->index >= s_->entries.size()) return {};
        return File("/"+s_->entries[s_->index++],FILE_READ);
    }
    void close() { s_.reset(); }
    size_t size() const {
        if(!s_ || !s_->file) return 0;
        auto pos=ftell(s_->file); fseek(s_->file,0,SEEK_END); auto n=ftell(s_->file); fseek(s_->file,pos,SEEK_SET); return n;
    }
    bool seek(size_t pos) { return s_ && s_->file && fseek(s_->file,pos,SEEK_SET)==0; }
    size_t read(uint8_t *data,size_t n) { return s_ && s_->file ? fread(data,1,n,s_->file) : 0; }
    size_t write(const uint8_t *data,size_t n) {
        if(!s_ || !s_->file) return 0;
        if(fakeWriteLimit < n) { n=fakeWriteLimit; s_->failed=true; }
        return fwrite(data,1,n,s_->file);
    }
    void flush() { if(s_ && s_->file)fflush(s_->file); }
    bool getWriteError() const { return s_ && s_->failed; }
};
class FakeFS {
public:
    bool begin(bool) { return fakeMount; }
    bool format() {
        ++fakeFormats;
        for(const auto &name:fakeNames()) std::remove((fakeRoot+"/"+name).c_str());
        fakeMount=true; return true;
    }
    File open(const String &name,const char *mode=FILE_READ) { return File(name.c_str(),mode); }
    bool remove(const String &name) { return std::remove((fakeRoot+name.c_str()).c_str())==0; }
    size_t usedBytes() const {
        size_t size=0; struct stat st{};
        for(const auto &name:fakeNames()) if(stat((fakeRoot+"/"+name).c_str(),&st)==0)size+=st.st_size;
        return size;
    }
    size_t totalBytes() const { return fakeCapacity; }
};
extern FakeFS LittleFS;
// Production uses /littlefs. Redirect ONLY that mount into the test fixture.
inline int fakeTruncate(const char *name,size_t size) {
    if(strncmp(name,"/littlefs/",10)!=0)return -1;
    FILE *f=fopen((fakeRoot+std::string(name+9)).c_str(),"r+b");
    if(!f)return -1;
#ifdef _WIN32
    int result=_chsize(_fileno(f),size);
#else
    int result=ftruncate(fileno(f),size);
#endif
    fclose(f); return result;
}
#define truncate fakeTruncate
