// SPDX-License-Identifier: MIT
// Copyright (c) 2026 bkwoka
// Repository: https://github.com/bkwoka/UUIDv7

#pragma once

#define UUID7_LIB_VERSION "1.2.1"

#include <stddef.h>
#include <stdint.h>

#if !defined(ARDUINO)
#include <ostream>
#endif

#include "UUID7Types.h"
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

// Auto-enable optimization for AVR architecture to save Flash/Cycles
#if (defined(ARDUINO_ARCH_AVR) || defined(__AVR__)) &&                         \
    !defined(UUID7_NO_OPTIMIZE)
#define UUID7_OPTIMIZE_SIZE
#endif

#if defined(ARDUINO)
#include <Arduino.h>
#endif

#if defined(ARDUINO)
class UUID7 : public Printable {
#else
class UUID7 {
#endif
public:
  typedef uuid7::fill_random_fn fill_random_fn;
  typedef uuid7::now_ms_fn now_ms_fn;

  // State Persistence Callbacks
  typedef uuid7::uuid_save_fn uuid_save_fn;
  typedef uuid7::uuid_load_fn uuid_load_fn;

  // Thread Safety Callbacks
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
   * @brief Internal helper for entropy fallback to access the private pin.
   */
  int16_t setEntropyAnalogPinFallback() const { return _entropyAnalogPin; }

  /**
   * @brief Inject custom thread lock/unlock callbacks (e.g., for FreeRTOS).
   */
  void setLockCallbacks(lock_fn_t lock_cb, lock_fn_t unlock_cb) {
    _lock_cb = lock_cb;
    _unlock_cb = unlock_cb;
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
  void mixEntropy(uint64_t seed) noexcept { _entropy_mixer = seed; }

  // --- GETTERS ---

  /**
   * @brief Get current configured UUID version.
   * @return Current version (e.g., UUID_VERSION_7).
   */
  UUIDVersion getVersion() const noexcept { return _version; }

  /**
   * @brief Get the variant of the generated UUID.
   * @return Variant bits shifted to right (should be 2 for RFC 4122).
   */
  uint8_t getVariant() const noexcept { return (_b[8] >> 6) & 0x03; }

  /** @brief Check if the currently generated UUID is Version 7. */
  bool isV7() const noexcept { return ((_b[6] >> 4) & 0x0F) == 7; }

  /** @brief Check if the currently generated UUID is Version 4. */
  bool isV4() const noexcept { return ((_b[6] >> 4) & 0x0F) == 4; }

  /**
   * @brief Check if the object contains a valid, generated UUID.
   * @return true if the internal buffer holds a Version 7 or Version 4 UUID.
   * @note Returns false for a freshly constructed object (before generate()),
   *       for an all-zero buffer, and for UUIDs of other versions (v1, v3, v5)
   *       loaded via fromBytes() or parseFromString(), as this library only
   *       generates and recognises v4 and v7.
   */
  bool isValid() const noexcept { return isV7() || isV4(); }

  /**
   * @brief Extract the 48-bit Unix timestamp (milliseconds) from the UUID.
   * @return Timestamp in milliseconds, or 0 if the UUID is not Version 7.
   */
  uint64_t getTimestamp() const noexcept;

  // --- INSTANCE PARSER ---

  /**
   * @brief Parse a 36-character UUID string directly into this object.
   * @param str36 Source string.
   * @return true if string is valid and parsed, false otherwise.
   */
  bool parse(const char *str36) noexcept { return parseFromString(str36, _b); }

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
   * @param auto_save_interval_ms How often to write state (Wear Leveling).
   * Default 10000ms.
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
  bool generate();

  /**
   * @brief Import 16 raw bytes into the UUID object.
   * @param bytes Source 16-byte array.
   */
  void fromBytes(const uint8_t bytes[16]) noexcept { memcpy(_b, bytes, 16); }

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
   * @brief Parse a 36-character UUID string into 16-byte binary format.
   * @param str36 Source string.
   * @param out Destination 16-byte array.
   * @return true if string is valid and parsed, false otherwise.
   */
  static bool parseFromString(const char *str36, uint8_t out[16]) noexcept;

  // --- Default Platform Implementations ---
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

  // --- Comparison and Logic Operators ---

  /** @brief Check if two UUIDs are identical. */
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

  // State for Monotonicity
  TimestampState _tsState;

  // Persistence State
  UUID7PersistenceState _persistence;

  uint64_t _entropy_mixer;

  // Runtime Configuration
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
