// SPDX-License-Identifier: MIT
// Copyright (c) 2026 bkwoka
// Repository: https://github.com/bkwoka/UUIDv7

#include "UUID7.h"

#if defined(PLATFORMIO_ESP32) || defined(ARDUINO_ARCH_ESP32)
#include "esp_timer.h"
#elif defined(PLATFORMIO_NATIVE)
#include <chrono>
#endif

uint64_t UUID7::default_now_ms(void *ctx) noexcept {
  (void)ctx;
#if defined(PLATFORMIO_NATIVE)
  using namespace std::chrono;
  return (uint64_t)duration_cast<milliseconds>(
             system_clock::now().time_since_epoch())
      .count();
#elif defined(PLATFORMIO_ESP32) || defined(ARDUINO_ARCH_ESP32)
  // ESP32 provides a native 64-bit microsecond timer. Overflow occurs in
  // ~292,000 years.
  return (uint64_t)(esp_timer_get_time() / 1000ULL);
#elif defined(ARDUINO)
  // Fallback for AVR, ESP8266, STM32, and RP2040.
  // Standard millis() returns a uint32_t which overflows after
  // approximately 49.7 days. A static accumulator is used to extend the value
  // to 64 bits.
  //
  // NOTE: These static variables are accessed outside the spinlock.
  // On single-core platforms (AVR, ESP8266, STM32) this is safe.
  // On RP2040 dual-core, a concurrent read/write of the 64-bit s_epoch_offset
  // can result in a "torn read" (as Cortex-M0+ lacks 64-bit atomic instructions),
  // or a double increment during wraparound. The result
  // is a ~99-day forward jump, which triggers clock-regression fallback
  // (UUIDv4 for that call). Probability is negligible in practice.
  static uint32_t s_prev_ms = 0;
  static uint64_t s_epoch_offset = 0;

  uint32_t now = millis();
  if (now < s_prev_ms) {
    // Increment epoch offset by 2^32 milliseconds
    s_epoch_offset += 0x100000000ULL;
  }
  s_prev_ms = now;

  return s_epoch_offset + now;
#else
  return 0;
#endif
}
