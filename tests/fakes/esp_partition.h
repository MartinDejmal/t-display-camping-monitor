#pragma once
#include <cstddef>
#include <cstdint>
constexpr int ESP_OK = 0;
constexpr int ESP_PARTITION_TYPE_DATA = 1;
constexpr int ESP_PARTITION_SUBTYPE_DATA_SPIFFS = 0x82;
struct esp_partition_t { size_t size; };
extern bool fakeErased;
inline const esp_partition_t *esp_partition_find_first(int,int,const char*) {
    static esp_partition_t part{4096}; return &part;
}
inline int esp_partition_read(const esp_partition_t*,size_t,void *data,size_t n) {
    memset(data,fakeErased?0xff:0x00,n); return ESP_OK;
}
