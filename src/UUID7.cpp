// SPDX-License-Identifier: MIT
// Copyright (c) 2026 bkwoka
// Repository: https://github.com/bkwoka/UUIDv7

#include "UUID7.h"
#include "UUID7Codec.h"
#include "UUID7Guard.h"
#include <string.h>

#if defined(PLATFORMIO_ESP32) || defined(ARDUINO_ARCH_ESP32)
portMUX_TYPE _uuid_spinlock = portMUX_INITIALIZER_UNLOCKED;
#elif defined(ARDUINO_ARCH_RP2040) && defined(PICO_SDK_VERSION_MAJOR)
spin_lock_t *_uuid_rp2040_spinlock = nullptr;
__attribute__((constructor)) static void _uuid_init_spinlock() {
  int lock_num = spin_lock_claim_unused(false);
  if (lock_num >= 0) {
    _uuid_rp2040_spinlock = spin_lock_init(lock_num);
  }
}
#elif defined(PLATFORMIO_NATIVE)
#include <thread>
std::mutex _uuid_mutex;
static inline void yield() { std::this_thread::yield(); }
#else
#if !defined(ARDUINO)
static inline void yield() {}
#endif
#endif

// --- CLASS IMPLEMENTATION ---

UUID7::UUID7(fill_random_fn rng, void *rng_ctx, now_ms_fn now,
             void *now_ctx) noexcept
    : _version(UUID_VERSION_7), _overflowPolicy(UUID_OVERFLOW_FAIL_FAST),
      _rng(rng),
      // _rng_ctx defaults to 'this' so the static default_fill_random can
      // access instance variables (like _entropyAnalogPin) via a void* cast.
      _rng_ctx(rng ? rng_ctx : this), _now(now), _now_ctx(now_ctx),
      _entropy_mixer(0),
      _regressionThresholdMs(10000), _lock_cb(nullptr), _unlock_cb(nullptr) {
  memset(_b, 0, sizeof(_b));

#if defined(ARDUINO_ARCH_AVR) || defined(__AVR__)
#if defined(A0)
  _entropyAnalogPin = A0;
#else
  // Default to pin 14 (A0) for ATmega328P based boards (Uno/Nano)
  _entropyAnalogPin = 14;
#endif
#else
  _entropyAnalogPin = -1;
#endif
}

void UUID7::setVersion(UUIDVersion v) { _version = v; }

void UUID7::setStorage(uuid_load_fn load_fn, uuid_save_fn save_fn, void *ctx,
                       uint32_t auto_save_interval_ms) {
  _persistence.load = load_fn;
  _persistence.save = save_fn;
  _persistence.ctx = ctx;
  _persistence.interval_ms = auto_save_interval_ms;
}

void UUID7::load() {
  if (_persistence.load) {
    uint64_t saved_ts = _persistence.load(_persistence.ctx);
    if (saved_ts > 0) {
      _persistence.last_saved_ms = saved_ts;
      uint64_t target = saved_ts + _persistence.interval_ms;

      _tsState.set(target);
    }
  }
}



bool UUID7::_incrementRandom() noexcept {
  // Increment 74-bit random field (bytes 15 down to 6).
  // Respects version (v7) and variant (RFC) bit masking.
  for (int i = 15; i >= 9; i--) {
    if (++_b[i] != 0)
      return true;
  }

  // Increment byte 8 while preserving variant bits (top 2 bits)
  uint8_t b8 = _b[8] & 0x3F;
  b8++;
  _b[8] = (_b[8] & 0xC0) | (b8 & 0x3F);
  if ((b8 & 0x40) == 0)
    return true; // No overflow if carry didn't reach bit 6

  // Carry to byte 7
  if (++_b[7] != 0)
    return true;

  // Increment byte 6 while preserving version bits (top 4 bits)
  uint8_t b6 = _b[6] & 0x0F;
  b6++;
  _b[6] = (_b[6] & 0xF0) | (b6 & 0x0F);

  if ((b6 & 0x10) == 0)
    return true; // No overflow if carry didn't reach bit 4

  return false;
}
bool UUID7::generate() {
  fill_random_fn rng = _rng ? _rng : &UUID7::default_fill_random;

  if (_version == UUID_VERSION_4) {
    rng(_b, sizeof(_b), _rng_ctx);
    uint8_t sum = 0;
    for (size_t i = 0; i < sizeof(_b); i++)
      sum |= _b[i];
    if (sum == 0)
      return false;

    _b[6] = (_b[6] & 0x0F) | 0x40; // Set UUID version to 4 (Random)
    _b[8] = (_b[8] & 0x3F) | 0x80; // Set variant to RFC 4122 (10b)
    return true;
  }

  now_ms_fn now_func = _now ? _now : &UUID7::default_now_ms;
  bool overflow_state = false;

  while (true) {
    uint8_t temp_rand[16];
    rng(temp_rand, sizeof(temp_rand), _rng_ctx);

    uint8_t sum = 0;
    for (size_t i = 0; i < sizeof(temp_rand); i++)
      sum |= temp_rand[i];
    if (sum == 0)
      return false;

    // Fetch current time outside the critical section to prevent potential
    // deadlocks with time providers that use blocking I/O (e.g., I2C RTC or NTP
    // sync).
    uint64_t now_ms = now_func(_now_ctx);
    if (now_ms == 0)
      return false;

    bool save_needed = false;
    uint64_t ts_to_save = 0;
    bool success = false;
    bool overflow_occurred = false;

    {
      // Ensure thread-safe access to monotonicity state
      UUID7Guard lock(_lock_cb, _unlock_cb);

      if (_entropy_mixer != 0) {
        for (int i = 0; i < 8; i++) {
          temp_rand[8 + i] ^= (uint8_t)(_entropy_mixer >> (i * 8));
        }
      }

      int cmp = _tsState.compare(now_ms);
      bool major_regression = false;

      if (cmp < 0) {
        if (now_ms + _regressionThresholdMs < _tsState.get()) {
          major_regression = true;
        } else {
          // Minor clock regression: Clamp to the last seen timestamp
          now_ms = _tsState.get();
          cmp = 0; // Force entry into same-millisecond logic
        }
      }

      if (major_regression) {
        // Major clock regression: Fall back to UUIDv4-like random generation
        temp_rand[6] = (temp_rand[6] & 0x0F) | 0x40; // v4 bits
        temp_rand[8] = (temp_rand[8] & 0x3F) | 0x80; // variant bits
        memcpy(_b, temp_rand, 16);
        return true;
      }

      if (cmp > 0) {
        _tsState.set(now_ms);
        memcpy(_b, temp_rand, 16);
        success = true;
        overflow_state = false;
      } else {
        if (overflow_state) {
          overflow_occurred = true;
        } else {
          bool initialized = (_b[6] & 0xF0) == 0x70;
          if (!initialized) {
            memcpy(_b, temp_rand, 16);
            success = true;
          } else {
            // Increment internal counter for same-millisecond monotonicity.
            if (!_incrementRandom()) {
              overflow_occurred = true;
              overflow_state = true;
            } else {
              success = true;
            }
          }
        }
      }

      if (success) {
        _tsState.stampBytes(_b);
      }
      if (success && _persistence.save &&
          (now_ms > _persistence.last_saved_ms + _persistence.interval_ms)) {
        save_needed = true;
        _persistence.last_saved_ms = now_ms;
        ts_to_save = now_ms;
      }

      if (success) {
        _b[6] = (_b[6] & 0x0F) | ((uint8_t)_version << 4);
        _b[8] = (_b[8] & 0x3F) | 0x80;
      }
    }

    if (success) {
      if (save_needed && _persistence.save) {
        _persistence.save(ts_to_save, _persistence.ctx);
      }
      return true;
    }

    if (overflow_occurred) {
      if (_overflowPolicy == UUID_OVERFLOW_FAIL_FAST) {
        return false;
      } else {
#if defined(ARDUINO)
        delay(
            1); // On RTOS platforms (ESP32/RP2040) this yields to the scheduler
#else
        yield();
#endif
        continue;
      }
    }
    return false;
  }
}

bool UUID7::toString(char *out, size_t buflen, bool uppercase,
                     bool dashes) const noexcept {
  uint8_t local_b[16];
  {
    UUID7Guard lock(_lock_cb, _unlock_cb);
    memcpy(local_b, _b, 16);
  }
  return UUID7Codec::encode(local_b, out, buflen, uppercase, dashes);
}

bool UUID7::parseFromString(const char *str, uint8_t out[16]) noexcept {
  return UUID7Codec::decode(str, out);
}

uint64_t UUID7::getTimestamp() const noexcept {
  if (!isV7())
    return 0;
  uint64_t ts = 0;
  uint8_t snap[6];
  {
    UUID7Guard lock(_lock_cb, _unlock_cb);
    memcpy(snap, _b, 6);
  }
  for (int i = 0; i < 6; i++) {
    ts = (ts << 8) | snap[i];
  }
  return ts;
}
