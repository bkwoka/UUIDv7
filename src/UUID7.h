// SPDX-License-Identifier: MIT
// Copyright (c) 2026 bkwoka
// Repository: https://github.com/bkwoka/UUIDv7

#pragma once

#define UUID7_LIB_VERSION "1.3.2"

#include <stddef.h>
#include <stdint.h>

#if !defined(ARDUINO)
#include <ostream>
#endif

#include "UUID7Types.h"

// Auto-enable optimization for AVR architecture to save Flash/Cycles.
// MUST be defined before including TimestampState.h
#if (defined(ARDUINO_ARCH_AVR) || defined(__AVR__)) &&                         \
    !defined(UUID7_NO_OPTIMIZE)
#define UUID7_OPTIMIZE_SIZE
#endif

#include "UUID7Persistence.h"
#include "TimestampState.h"

#include <string.h>

#if defined(__has_cpp_attribute) && __has_cpp_attribute(deprecated)
#define UUID7_DEPRECATED(msg) [[deprecated(msg)]]
#elif defined(__GNUC__)
#define UUID7_DEPRECATED(msg) __attribute__((deprecated(msg)))
#else
#define UUID7_DEPRECATED(msg)
#endif

#if defined(__has_cpp_attribute) && __has_cpp_attribute(nodiscard)
#define UUID7_NODISCARD [[nodiscard]]
#elif defined(__GNUC__)
#define UUID7_NODISCARD __attribute__((warn_unused_result))
#else
#define UUID7_NODISCARD
#endif


#if defined(ARDUINO)
#include <Arduino.h>
#endif

/**
 * @warning Copying a UUID7 object copies its monotonicity state (_tsState).
 * Two copies driven independently will produce UUIDs that may compare
 * less-than each other — violating K-sortability. Use copies only as
 * read-only snapshots (comparison, serialization), never as separate generators.
 */
#if defined(ARDUINO)
class UUID7 : public Printable {
#else
class UUID7 {
#endif
public:
  typedef uuid7::fill_random_fn fill_random_fn;
  typedef uuid7::now_ms_fn now_ms_fn;

  typedef uuid7::uuid_save_fn uuid_save_fn;
  typedef uuid7::uuid_load_fn uuid_load_fn;

  typedef uuid7::lock_fn_t lock_fn_t;

  /**
   * @brief Set custom threshold for clock regression (default: 10000 ms).
   */
  void setRegressionThreshold(uint32_t ms) { _regressionThresholdMs = ms; }

  /**
   * @brief Set analog pin for entropy generation on AVR (default: A0).
   * Set to -1 to disable analog noise reading.
   */
  void setEntropyAnalogPin(int16_t pin) { _entropyAnalogPin = pin; }


  /**
   * @brief Inject custom thread lock/unlock callbacks (e.g., for FreeRTOS).
   */
  void setLockCallbacks(lock_fn_t lock_cb, lock_fn_t unlock_cb) {
    // Both callbacks must be provided, or neither. Prevents UB/Deadlocks.
    bool both = (lock_cb != nullptr) && (unlock_cb != nullptr);
    _lock_cb = both ? lock_cb : nullptr;
    _unlock_cb = both ? unlock_cb : nullptr;
  }

  /**
   * @brief Initialize generator with optional custom RNG and Time sources.
   * @param rng Pointer to random fill function (nullptr for default).
   * @param rng_ctx User context for RNG.
   * @param now Pointer to millisecond time function (nullptr for default).
   * @param now_ctx User context for time.
   */
  UUID7(fill_random_fn rng = nullptr, void *rng_ctx = nullptr,
        now_ms_fn now = nullptr, void *now_ctx = nullptr) noexcept;

  /**
   * @brief Set the UUID version to generate (v4 or v7).
   * @param v UUID version.
   */
  void setVersion(UUIDVersion v);

  /**
   * @brief Configure a custom time provider at runtime.
   * Useful if the clock (RTC/NTP) is initialized after the UUID object.
   * @param now Pointer to millisecond time function.
   * @param ctx User context (optional).
   */
  void setTimeProvider(now_ms_fn now, void *ctx = nullptr) {
    _now = now;
    _now_ctx = ctx;
  }

  void setRandomSource(fill_random_fn rng, void *ctx = nullptr) {
    _rng = rng;
    _rng_ctx = rng ? ctx : this;
  }

  /**
   * @brief [Deprecated] Configure a custom RNG source. Use setRandomSource
   * instead.
   */
  UUID7_DEPRECATED("Use setRandomSource() instead")
  inline void setRandomProvider(fill_random_fn rng, void *ctx = nullptr) {
    setRandomSource(rng, ctx);
  }

  /**
   * @brief Mix additional entropy into the UUID generation (e.g., MAC address,
   * hardware ID). This helps ensure uniqueness across different devices even if
   * timestamps collide.
   * @param seed 64-bit entropy seed.
   */
  void mixEntropy(uint64_t seed) noexcept;

  /**
   * @brief Get current configured UUID version.
   * @warning After a major clock regression, the generator temporarily falls back 
   * to emitting a v4 UUID to prevent collisions. In such cases, this method still 
   * returns the configured version (v7), while isV7() will return false. 
   * Always use isV7() / isV4() to inspect the actual state of the generated buffer.
   * @return Current configured version (e.g., UUID_VERSION_7).
   */
  UUIDVersion getVersion() const noexcept { return _version; }

  /**
   * @brief Get the variant of the generated UUID.
   * @return Variant bits shifted to right (should be 2 for RFC 4122).
   */
  uint8_t getVariant() const noexcept;

  /** @brief Check if the currently generated UUID is Version 7. */
  bool isV7() const noexcept;

  /** @brief Check if the currently generated UUID is Version 4. */
  bool isV4() const noexcept;

  /**
   * @brief Check if the object contains a valid, generated UUID.
   * @return true if the internal buffer holds a Version 7 or Version 4 UUID.
   * @note Returns false for a freshly constructed object (before generate()),
   *       for an all-zero buffer, and for UUIDs of other versions (v1, v3, v5)
   *       loaded via fromBytes() or parseFromString(), as this library only
   *       generates and recognises v4 and v7.
   * @warning This is a convenience method that calls isV7() and isV4() sequentially.
   *          In multi-threaded environments, it performs up to two independent lock cycles.
   */
  bool isValid() const noexcept { return isV7() || isV4(); }

  /**
   * @brief Extract the 48-bit Unix timestamp (milliseconds) from the UUID.
   * @return Timestamp in milliseconds, or 0 if the UUID is not Version 7.
   */
  uint64_t getTimestamp() const noexcept;

  /**
   * @brief Parse a UUID string directly into this object.
   * Supports both 36-character (dashed) and 32-character (undashed) formats.
   * @param str36 Source string (36 or 32 chars).
   * @return true if string is valid and parsed, false otherwise.
   */
  bool parse(const char *str36) noexcept;

  /**
   * @brief Configure behavior when sub-millisecond counter overflows.
   * @warning If policy is UUID_OVERFLOW_WAIT, do NOT call generate()
   * from an ISR or hardware interrupt handler.
   * @param policy Policy (Wait or Fail Fast).
   */
  void setOverflowPolicy(UUIDOverflowPolicy policy) {
    _overflowPolicy = policy;
  }

  /**
   * @brief Get current overflow policy.
   * @return Current policy.
   */
  UUIDOverflowPolicy getOverflowPolicy() const { return _overflowPolicy; }

  /**
   * @brief Configure persistence to handle reboots/clock resets.
   * @param load_fn Function to read uint64_t timestamp from NVS/EEPROM.
   * @param save_fn Function to write uint64_t timestamp to NVS/EEPROM.
   * @param ctx Context for callbacks.
   * @param auto_save_interval_ms Minimum write interval in ms (wear leveling).
   *        WARNING: Passing 0 will write on every generate() call, which can
   *        rapidly exhaust EEPROM/Flash endurance. Use 0 only for FRAM/SRAM.
   *        Default 10000ms.
   */
  void setStorage(uuid_load_fn load_fn, uuid_save_fn save_fn, void *ctx,
                  uint32_t auto_save_interval_ms = 10000);

  /**
   * @brief Load state from storage and apply "Safety Jump".
   * MUST be called in setup() if storage is configured.
   * Sets internal clock to: loaded_ts + auto_save_interval_ms.
   * This prevents collisions for the unsaved time window before a crash.
   */
  void load();

  /**
   * @brief Generate a new UUID based on current version setting.
   *
   * Thread Safety: Safe for ISR/RTOS if:
   * 1. The provided 'now_ms_fn' is ISR-safe (non-blocking).
   * 2. The provided 'fill_random_fn' is reentrant or thread-safe.
   *
   * Uses "Optimistic Read" strategy to minimize critical section duration.
   *
   * @return true if successful, false if time source is invalid (returns 0).
   */
  UUID7_NODISCARD bool generate();

  /**
   * @brief Import 16 raw bytes into the UUID object.
   * @param bytes Source 16-byte array.
   */
  void fromBytes(const uint8_t bytes[16]) noexcept;

  /**
   * @brief Format UUID as standard string.
   * @param out Destination buffer (must be >= 37 bytes for dashed, >= 33 for
   * raw).
   * @param buflen Length of destination buffer.
   * @param uppercase If true, uses UPPERCASE hex.
   * @param dashes If false, omits hyphens (32-char result).
   * @return true if successful, false if buffer is too small.
   */
  bool toString(char *out, size_t buflen, bool uppercase = false,
                bool dashes = true) const noexcept;

  /**
   * @brief Access raw 16 bytes of the current UUID.
   * @warning NOT THREAD-SAFE. The internal buffer may change if generate()
   * is called from another thread/ISR. In multi-threaded environments,
   * use toString() or copy the data immediately under your own lock.
   * @return Pointer to internal byte array.
   */
  const uint8_t *data() const noexcept { return _b; }

  /**
   * @brief Parse a UUID string into 16-byte binary format.
   * Supports both 36-character (dashed) and 32-character (undashed) formats.
   * @param str36 Source string (36 or 32 chars).
   * @param out Destination 16-byte array.
   * @return true if string is valid and parsed, false otherwise.
   */
  static bool parseFromString(const char *str36, uint8_t out[16]) noexcept;

  static void default_fill_random(uint8_t *dest, size_t len,
                                  void *ctx) noexcept;
  static uint64_t default_now_ms(void *ctx) noexcept;

#if defined(ARDUINO)
  size_t printTo(Print &p) const override {
    char buf[37];
    toString(buf, sizeof(buf));
    return p.print(buf);
  }
#endif

  /** 
   * @brief Check if two UUIDs are identical. 
   * @warning Relational operators read internal buffers without acquiring a lock.
   * Comparing UUID7 objects shared across threads without external synchronization
   * is undefined behavior. Use toString() for thread-safe comparisons.
   */
  bool operator==(const UUID7 &other) const {
    return memcmp(_b, other._b, 16) == 0;
  }

  /** @brief Check if two UUIDs are different. */
  bool operator!=(const UUID7 &other) const { return !(*this == other); }

  /** @brief Lexicographical comparison for sorting (K-Sortable property). */
  bool operator<(const UUID7 &other) const {
    return memcmp(_b, other._b, 16) < 0;
  }

  /** @brief Lexicographical comparison (Greater than). */
  bool operator>(const UUID7 &other) const { return other < *this; }

  /** @brief Lexicographical comparison (Less than or equal). */
  bool operator<=(const UUID7 &other) const { return !(other < *this); }

  /** @brief Lexicographical comparison (Greater than or equal). */
  bool operator>=(const UUID7 &other) const { return !(*this < other); }

#if !defined(ARDUINO)
  /**
   * @brief OStream operator for non-Arduino environments (std::cout support).
   */
  friend std::ostream &operator<<(std::ostream &os, const UUID7 &uuid) {
    char buf[37];
    uuid.toString(buf, sizeof(buf));
    os << buf;
    return os;
  }
#endif

private:
  uint8_t _b[16];
  UUIDVersion _version;
  UUIDOverflowPolicy _overflowPolicy;
  fill_random_fn _rng;
  void *_rng_ctx;
  now_ms_fn _now;
  void *_now_ctx;

  TimestampState _tsState;

  UUID7PersistenceState _persistence;

  uint64_t _entropy_mixer;

  uint32_t _regressionThresholdMs;
  int16_t _entropyAnalogPin;
  lock_fn_t _lock_cb;
  lock_fn_t _unlock_cb;

  /**
   * @brief Internal helper to increment the random part (74 bits) for
   * monotonicity.
   * @return true if increment succeeded, false if overflow occurred.
   */
  bool _incrementRandom() noexcept;
};
