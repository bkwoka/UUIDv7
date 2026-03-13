// SPDX-License-Identifier: MIT
// Copyright (c) 2026 bkwoka
// Repository: https://github.com/bkwoka/UUIDv7

#pragma once

#include <stddef.h>
#include <stdint.h>

enum UUIDVersion { 
    UUID_VERSION_4 = 4, 
    UUID_VERSION_7 = 7 
};

enum UUIDOverflowPolicy {
    UUID_OVERFLOW_FAIL_FAST,
    UUID_OVERFLOW_WAIT
};

namespace uuid7 {
    typedef void (*fill_random_fn)(uint8_t *dest, size_t len, void *ctx);
    typedef uint64_t (*now_ms_fn)(void *ctx);
    typedef void (*uuid_save_fn)(uint64_t timestamp, void *ctx);
    typedef uint64_t (*uuid_load_fn)(void *ctx);
    typedef void (*lock_fn_t)(void);
}
