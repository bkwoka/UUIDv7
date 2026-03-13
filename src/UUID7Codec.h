// SPDX-License-Identifier: MIT
// Copyright (c) 2026 bkwoka
// Repository: https://github.com/bkwoka/UUIDv7

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

struct UUID7Codec {
    static inline bool encode(const uint8_t bytes[16], char *out, size_t buflen,
                              bool uppercase = false, bool dashes = true) noexcept {
        size_t required = dashes ? 37 : 33;
        if (!out || buflen < required) {
            return false;
        }

        static const char hexLower[] = "0123456789abcdef";
        static const char hexUpper[] = "0123456789ABCDEF";
        const char *hex = uppercase ? hexUpper : hexLower;

        const uint8_t *p = bytes;
        char *s = out;

        for (int i = 0; i < 16; i++) {
            if (dashes && (i == 4 || i == 6 || i == 8 || i == 10)) {
                *s++ = '-';
            }
            *s++ = hex[(*p >> 4) & 0x0F];
            *s++ = hex[*p++ & 0x0F];
        }

        *s = '\0';
        return true;
    }

    static inline bool decode(const char *str, uint8_t out[16]) noexcept {
        if (!str || !out) {
            return false;
        }

        size_t len = strlen(str);
        bool dashed;

        if (len == 36) {
            dashed = true;
        } else if (len == 32) {
            dashed = false;
        } else {
            return false;
        }

        const char *p = str;

        auto hexval =[](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return 10 + c - 'a';
            if (c >= 'A' && c <= 'F') return 10 + c - 'A';
            return -1;
        };

        for (int i = 0; i < 16; i++) {
            if (dashed && (i == 4 || i == 6 || i == 8 || i == 10)) {
                if (*p != '-') return false;
                p++;
            }

            int hi = hexval(*p++);
            int lo = hexval(*p++);

            if (hi < 0 || lo < 0) return false;

            out[i] = (uint8_t)((hi << 4) | lo);
        }

        return true;
    }
};
