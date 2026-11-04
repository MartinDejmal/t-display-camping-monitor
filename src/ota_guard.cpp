#include "ota_guard.h"
#include <Preferences.h>
#include <esp_ota_ops.h>
#include <esp_task_wdt.h>
namespace {
struct Trial { uint32_t previous, target, attempted, marker; };
constexpr uint32_t MARKER = 0x4f544132;
bool writeTrial(const Trial &t) {
    Preferences p;
    if (!p.begin("ota-guard", false)) return false;
    bool ok = p.putBytes("trial", &t, sizeof(t)) == sizeof(t);
    p.end();
    return ok;
}
}
void OtaGuard::begin() {
    Preferences p;
    if (!p.begin("ota-guard", false)) { error_ = "OTA guard NVS unavailable"; return; }
    Trial t{};
    p.getBytes("trial", &t, sizeof(t));
    p.end();
    const auto *running = esp_ota_get_running_partition();
    if (t.marker != MARKER || !running) return;
    if (running->address != t.target) { cancel(); return; }
    if (t.attempted) {
        const esp_partition_t *previous = nullptr;
        esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, nullptr);
        while (it) {
            const auto *part = esp_partition_get(it);
            if (part->address == t.previous) { previous = part; break; }
            it = esp_partition_next(it);
        }
        if (it) esp_partition_iterator_release(it);
        if (previous && esp_ota_set_boot_partition(previous) == ESP_OK) {
            // Keep the marker until the previous app has actually booted.
            ESP.restart();
        }
        error_ = "Cannot select previous firmware";
        return;
    }
    t.attempted = 1;
    if (!writeTrial(t)) { error_ = "Cannot arm OTA boot guard"; return; }
    trial_ = true;
    esp_task_wdt_init(60, true);
    esp_task_wdt_add(nullptr);
}
bool OtaGuard::stage() {
    const auto *running = esp_ota_get_running_partition();
    const auto *next = esp_ota_get_next_update_partition(nullptr);
    if (!running || !next || trial_) return false;
    return writeTrial({running->address, next->address, 0, MARKER});
}
bool OtaGuard::cancel() {
    Preferences p;
    if (!p.begin("ota-guard", false)) { error_ = "Cannot clear OTA guard"; return false; }
    bool ok = !p.isKey("trial") || p.remove("trial");
    p.end();
    if (!ok) { error_ = "Cannot clear OTA guard"; return false; }
    trial_ = false;
    return true;
}
void OtaGuard::confirm() {
    if (!trial_) return;
    // This Arduino bootloader does not enable IDF automatic rollback.
    // Our NVS guard rolls back after an unconfirmed trial resets.
    if (!cancel()) return;
    esp_task_wdt_delete(nullptr);
}
