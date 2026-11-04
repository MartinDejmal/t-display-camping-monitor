#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>
// Marker begins/ends with '|', with no internal '|'. State spans upload chunks.
inline void feedOtaMarker(const uint8_t *bytes, size_t size, const char *marker,
                          size_t &matched, bool &found) {
    const size_t length = strlen(marker);
    for (size_t i = 0; i < size && !found; ++i) {
        const char c = static_cast<char>(bytes[i]);
        if (c == marker[matched]) ++matched;
        else matched = c == marker[0] ? 1 : 0;
        if (matched == length) found = true;
    }
}
