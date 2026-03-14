// SPDX-License-Identifier: MIT
// Copyright (c) 2026 bkwoka
// Repository: https://github.com/bkwoka/UUIDv7

#include "UUID7.h"
#include "UUID7Guard.h"

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
  static uint32_t s_prev_ms = 0;
  static uint64_t s_epoch_offset = 0;

  uint32_t now = millis();
  uint64_t result;
  {
    // Protect static accumulator from concurrent access on multi-core (RP2040)
    // or preemptive RTOS environments (STM32 FreeRTOS).
    UUID7Guard lock(nullptr, nullptr);
    if (now < s_prev_ms) {
      // Increment epoch offset by 2^32 milliseconds
      s_epoch_offset += 0x100000000ULL;
    }
    s_prev_ms = now;
    result = s_epoch_offset + now;
  }

  return result;
#else
  #warning "UUID7: No clock source detected for this platform. Inject a time provider via setTimeProvider() or generate() will always return false."
  return 0;
#endif
}
