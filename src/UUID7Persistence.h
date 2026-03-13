// SPDX-License-Identifier: MIT
// Copyright (c) 2026 bkwoka
// Repository: https://github.com/bkwoka/UUIDv7

#pragma once
#include "UUID7Types.h"

struct UUID7PersistenceState {
    uuid7::uuid_load_fn load;
    uuid7::uuid_save_fn save;
    void *ctx;
    uint32_t interval_ms;
    uint64_t last_saved_ms;

    UUID7PersistenceState() noexcept
        : load(nullptr), save(nullptr), ctx(nullptr),
          interval_ms(10000), last_saved_ms(0) {}
};
