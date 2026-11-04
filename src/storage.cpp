#include "storage.h"
#include "config.h"
#include <LittleFS.h>
#include <esp_partition.h>
#include <algorithm>
#include <unistd.h>

namespace {
bool partitionErased() {
    const esp_partition_t *part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_DATA_SPIFFS, "spiffs");
    if (!part) return false;
    uint8_t buffer[1024];
    for (size_t pos = 0; pos < part->size; pos += sizeof(buffer)) {
        if (esp_partition_read(part, pos, buffer, sizeof(buffer)) != ESP_OK) return false;
        for (uint8_t value : buffer) if (value != 0xff) return false;
        yield();
    }
    return true;
}
}
String Storage::path(uint32_t block) const {
    char name[24];
    snprintf(name, sizeof(name), "/b%08lu.bin", static_cast<unsigned long>(block));
    return name;
}
bool Storage::begin() {
    if (!LittleFS.begin(false)) {
        // Provision a genuinely erased partition only. Never erase corrupt data.
        if (!partitionErased() || !LittleFS.format() || !LittleFS.begin(false)) {
            error_ = "History mount failed; no automatic format";
            return false;
        }
    }
    File root = LittleFS.open("/");
    File file;
    while ((file = root.openNextFile())) {
        String name = file.name();
        if (name.startsWith("/")) name.remove(0, 1);
        if (name.length() == 13 && name[0] == 'b' && name.endsWith(".bin")) {
            uint32_t id = 0;
            bool digits = true;
            for (int i = 1; i <= 8; ++i) {
                if (name[i] < '0' || name[i] > '9') digits = false;
                id = id * 10 + name[i] - '0';
            }
            if (digits) blocks_.push_back(id);
        }
        file.close();
        if (blocks_.size() > 512) {
            error_ = "Too many history files";
            return false;
        }
    }
    root.close();
    std::sort(blocks_.begin(), blocks_.end());
    if (!trimTail()) return false;
    ready_ = true;
    return true;
}
bool Storage::trimTail() {
    if (blocks_.empty()) return true;
    String name = path(blocks_.back());
    File f = LittleFS.open(name, FILE_READ);
    if (!f) { error_ = "Cannot read history tail"; return false; }
    const size_t original = f.size();
    size_t size = original - original % sizeof(logger::Record);
    logger::Record r;
    while (size) {
        f.seek(size - sizeof(r));
        if (f.read(reinterpret_cast<uint8_t *>(&r), sizeof(r)) != sizeof(r)) {
            f.close(); error_ = "History read failed"; return false;
        }
        if (logger::valid(r)) break;
        // A complete record of a different version is not a torn write.
        if ((r.flags & 0xf000) != logger::RECORD_VERSION &&
            r.crc == logger::crc16(reinterpret_cast<uint8_t *>(&r), offsetof(logger::Record, crc))) {
            f.close(); error_ = "Unsupported history version"; return false;
        }
        ++corrupt_;
        size -= sizeof(r);
    }
    f.close();
    if (size != original && truncate((String("/littlefs") + name).c_str(), size) != 0) {
        error_ = "Cannot repair incomplete history tail";
        return false;
    }
    return true;
}
bool Storage::retireOldest() {
    if (blocks_.size() <= 1) return false;
    if (!LittleFS.remove(path(blocks_.front()))) return false;
    blocks_.erase(blocks_.begin());
    return true;
}
void Storage::prune(const logger::Record &now) {
    while (blocks_.size() > 1) {
        File f = LittleFS.open(path(blocks_.front()), FILE_READ);
        logger::Record last;
        bool expired = false;
        if (f && f.size() >= sizeof(last)) {
            f.seek(f.size() - sizeof(last));
            if (f.read(reinterpret_cast<uint8_t *>(&last), sizeof(last)) == sizeof(last) && logger::valid(last)) {
                uint32_t age;
                expired = logger::ageSeconds(last, now.session, now.elapsedS, now.epoch, age) && age > HISTORY_SECONDS;
            }
        }
        f.close();
        if (!expired && used() < capacity() * 9 / 10 && blocks_.size() < 500) break;
        if (!retireOldest()) break;
    }
}
bool Storage::append(logger::Record record) {
    if (!ready_) return false;
    logger::seal(record);
    if (blocks_.empty()) blocks_.push_back(1);
    File existing = LittleFS.open(path(blocks_.back()), FILE_READ);
    bool full = existing && existing.size() >= BLOCK_RECORDS * sizeof(record);
    existing.close();
    if (full) {
        if (blocks_.back() >= 99999999UL) { error_ = "History sequence exhausted"; return false; }
        blocks_.push_back(blocks_.back() + 1);
    }
    prune(record);
    File f = LittleFS.open(path(blocks_.back()), FILE_APPEND);
    if (!f) { error_ = "Cannot open history for append"; return false; }
    const size_t n = f.write(reinterpret_cast<const uint8_t *>(&record), sizeof(record));
    f.flush();
    bool ok = n == sizeof(record) && !f.getWriteError();
    f.close();
    if (!ok) {
        error_ = "History write failed";
        ready_ = false; // repair once on reboot, never append after a partial write
    } else error_ = "";
    return ok;
}
void Storage::visit(const std::function<bool(const logger::Record &)> &callback) {
    if (!ready_) return;
    const auto snapshot = blocks_;
    for (auto id : snapshot) {
        File f = LittleFS.open(path(id), FILE_READ);
        if (!f) continue;
        const size_t limit = f.size() / sizeof(logger::Record);
        logger::Record r;
        for (size_t i = 0; i < limit; ++i) {
            if (f.read(reinterpret_cast<uint8_t *>(&r), sizeof(r)) != sizeof(r)) break;
            if (logger::valid(r) && !callback(r)) { f.close(); return; }
            if ((i & 63) == 0) yield();
        }
        f.close();
    }
}
bool Storage::latest(logger::Record &r) {
    if (!ready_) return false;
    for (auto it = blocks_.rbegin(); it != blocks_.rend(); ++it) {
        File f = LittleFS.open(path(*it), FILE_READ);
        for (size_t pos = f ? f.size() / sizeof(r) : 0; pos > 0; --pos) {
            f.seek((pos - 1) * sizeof(r));
            if (f.read(reinterpret_cast<uint8_t *>(&r), sizeof(r)) == sizeof(r) && logger::valid(r)) return true;
        }
    }
    return false;
}
void Storage::history(const ClockState &clock, uint32_t seconds, HistoryBin *bins, size_t count) {
    for (size_t i = 0; i < count; ++i) bins[i] = {};
    const auto session = clock.session(), elapsed = clock.elapsedS(), epoch = clock.epoch();
    visit([&](const logger::Record &r) {
        uint32_t age;
        if (!logger::ageSeconds(r, session, elapsed, epoch, age) || age >= seconds) return true;
        const size_t index = (static_cast<uint64_t>(seconds - 1 - age) * count) / seconds;
        HistoryBin &b = bins[index];
        if (!(r.flags & logger::FLAG_SENSOR_OK)) { b.gap = true; return true; }
        if (b.valid && b.session != r.session) b.gap = true;
        if (!b.valid) {
            b.minT = b.maxT = r.temperature;
            b.session = r.session;
            b.valid = true;
        } else {
            b.minT = std::min(b.minT, r.temperature);
            b.maxT = std::max(b.maxT, r.temperature);
        }
        if (!logger::hasHumidity(r)) b.humidityGap = true;
        else if (!b.humidityValid) {
            b.minH = b.maxH = r.humidity;
            b.humidityValid = true;
        } else {
            b.minH = std::min(b.minH, r.humidity);
            b.maxH = std::max(b.maxH, r.humidity);
        }
        return true;
    });
}
size_t Storage::used() const { return ready_ ? LittleFS.usedBytes() : 0; }
size_t Storage::capacity() const { return ready_ ? LittleFS.totalBytes() : 0; }
