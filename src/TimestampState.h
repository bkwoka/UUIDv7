// SPDX-License-Identifier: MIT
// Copyright (c) 2026 bkwoka
// Repository: https://github.com/bkwoka/UUIDv7

#pragma once
#include <stdint.h>
#include <string.h>

#ifdef UUID7_OPTIMIZE_SIZE

struct TimestampState {
    uint8_t bytes[6];

    TimestampState() noexcept { memset(bytes, 0, 6); }

    void set(uint64_t ms) noexcept {
        uint64_t v = ms & 0x0000FFFFFFFFFFFFULL;
        bytes[5] = v & 0xFF; v >>= 8;
        bytes[4] = v & 0xFF; v >>= 8;
        bytes[3] = v & 0xFF; v >>= 8;
        bytes[2] = v & 0xFF; v >>= 8;
        bytes[1] = v & 0xFF; v >>= 8;
        bytes[0] = v & 0xFF;
    }

    uint64_t get() const noexcept {
        uint64_t v = 0;
        for (int i = 0; i < 6; i++) v = (v << 8) | bytes[i];
        return v;
    }

    int compare(uint64_t ms) const noexcept {
        uint8_t tmp[6];
        uint64_t v = ms & 0x0000FFFFFFFFFFFFULL;
        tmp[5] = v & 0xFF; v >>= 8;
        tmp[4] = v & 0xFF; v >>= 8;
        tmp[3] = v & 0xFF; v >>= 8;
        tmp[2] = v & 0xFF; v >>= 8;
        tmp[1] = v & 0xFF; v >>= 8;
        tmp[0] = v & 0xFF;
        return memcmp(tmp, bytes, 6);
    }

    void stampBytes(uint8_t* dest) const noexcept {
        memcpy(dest, bytes, 6);
    }
};

#else

struct TimestampState {
    uint64_t ms;

    TimestampState() noexcept : ms(0) {}

    void set(uint64_t new_ms) noexcept { ms = new_ms; }
    uint64_t get() const noexcept { return ms; }

    int compare(uint64_t new_ms) const noexcept {
        if (new_ms > ms) return 1;
        if (new_ms < ms) return -1;
        return 0;
    }

    void stampBytes(uint8_t* dest) const noexcept {
        uint64_t v = ms & 0x0000FFFFFFFFFFFFULL;
        dest[5] = v & 0xFF; v >>= 8;
        dest[4] = v & 0xFF; v >>= 8;
        dest[3] = v & 0xFF; v >>= 8;
        dest[2] = v & 0xFF; v >>= 8;
        dest[1] = v & 0xFF; v >>= 8;
        dest[0] = v & 0xFF;
    }
};

#endif
