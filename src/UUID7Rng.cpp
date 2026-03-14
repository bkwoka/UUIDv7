// SPDX-License-Identifier: MIT
// Copyright (c) 2026 bkwoka
// Repository: https://github.com/bkwoka/UUIDv7

#include "UUID7.h"
#include "UUID7Prng.h"
#include "UUID7Guard.h"
#include <string.h>

#if defined(PLATFORMIO_ESP32) || defined(ARDUINO_ARCH_ESP32)
#include "esp_system.h"
#elif defined(PLATFORMIO_ESP8266) || defined(ESP8266)
extern "C" {
#include "user_interface.h"
}
#elif defined(ARDUINO_ARCH_RP2040)
#include <hardware/structs/rosc.h>
#elif defined(PLATFORMIO_NATIVE)
#include <random>
#endif

void UUID7::default_fill_random(uint8_t *dest, size_t len, void *ctx) noexcept {
#if defined(PLATFORMIO_ESP32) || defined(ARDUINO_ARCH_ESP32)
  (void)ctx;
  // ESP32 Hardware RNG
  size_t i = 0;
  while (i + 4 <= len) {
    uint32_t r = esp_random();
    memcpy(dest + i, &r, 4);
    i += 4;
  }
  if (i < len) {
    uint32_t r = esp_random();
    while (i < len) {
      dest[i++] = (uint8_t)(r & 0xFF);
      r >>= 8;
    }
  }

#elif defined(PLATFORMIO_ESP8266) || defined(ESP8266)
  (void)ctx;
  // ESP8266 Hardware RNG (WDEV)
  os_get_random((unsigned char *)dest, len);

#elif defined(ARDUINO_ARCH_RP2040)
  (void)ctx;
  // RP2040 Hardware ROSC (Ring Oscillator)
  // Supported on both Earle Philhower and MBED cores.
  // Provides high-entropy jitter but requires multiple clock cycles per bit.
  for (size_t i = 0; i < len; i++) {
    uint8_t r = 0;
    for (int bit = 0; bit < 8; bit++) {
      r = (r << 1) | (rosc_hw->randombit & 1);
    }
    dest[i] = r;
  }

#elif defined(ARDUINO_ARCH_STM32)
  (void)ctx;
  /*
   * STM32 Entropy Strategy:
   * Combines multiple hardware/temporal sources to maximize uniqueness
   * even on devices lacking a dedicated TRNG peripheral.
   */

  // 1. Initial platform-provided randomness
  {
    // Protect Arduino's non-reentrant random() from concurrent RTOS tasks
    UUID7Guard lock(nullptr, nullptr);
    for (size_t i = 0; i < len; i++) {
      dest[i] = (uint8_t)random(256);
    }
  }

  // 2. Hardware-Unique Identification (UID Mix)
  // Incorporates the 96-bit Unique Device ID to ensure spatial uniqueness.
  uint32_t uid[3];
  uid[0] = HAL_GetUIDw0();
  uid[1] = HAL_GetUIDw1();
  uid[2] = HAL_GetUIDw2();

  for (size_t i = 0; i < len; i++) {
    dest[i] ^= (uint8_t)(uid[i % 3] >> ((i % 4) * 8));
  }

  // 3. Jitter Mix: Temporal entropy from high-resolution SysTick value
  uint32_t tick = SysTick->VAL;
  dest[0] ^= (uint8_t)(tick & 0xFF);
  dest[len - 1] ^= (uint8_t)((tick >> 8) & 0xFF);

#elif defined(ARDUINO_ARCH_AVR) || defined(__AVR__)
  // ctx is intentionally used here to access _entropyAnalogPin via UUID7* cast.
#warning                                                                       \
    "UUID7: Using fallback entropy (ADC noise + Clock Jitter). Not cryptographically secure."

  // Shared PRNG state across all UUID7 instances on AVR.
  // This is a deliberate design choice: it ensures that multiple instances
  // draw from a single, continuous entropy pool, preventing identical UUID
  // sequences if multiple objects are instantiated simultaneously.
  static uint32_t rng_state = 0;

  if (rng_state == 0) {
    uint32_t entropy = 0;
    volatile uint8_t stack_var;
    entropy ^= uuid_mix32((uint16_t)(uintptr_t)&stack_var);
    entropy ^= uuid_mix32((uint32_t)((uintptr_t)default_fill_random));
    entropy ^= uuid_mix32(micros());

    int16_t analog_pin =
        ctx ? static_cast<UUID7 *>(ctx)->_entropyAnalogPin : -1;
    if (analog_pin >= 0) {
      for (int i = 0; i < 4; i++) {
        (void)analogRead(analog_pin);
      }
      for (int i = 0; i < 8; i++) {
        unsigned long t_start = micros();
        uint16_t val = analogRead(analog_pin);
        unsigned long t_end = micros();
        entropy = uuid_mix32(entropy ^ val);
        entropy = uuid_mix32(entropy ^ (t_end - t_start));
        delayMicroseconds(10 + (val & 0x0F));
      }
    }

    for (int i = 0; i < 4; i++) {
      unsigned long t1 = micros();
      for (volatile uint8_t k = 0; k < (uint8_t)((t1 & 0x07) * 10); k++) {
        __asm__("nop");
      }
      unsigned long t2 = micros();
      entropy ^= uuid_mix32(t2 ^ t1);
    }

    if (entropy == 0)
      entropy = 0xBADC0FFE;
    rng_state = entropy;
  }

  size_t i = 0;
  while (i < len) {
    uint32_t r;
    // Protect PRNG state from concurrent ISR access on 8-bit AVR
    uint8_t oldSREG = SREG;
    cli();
    rng_state ^= (uint32_t)micros();
    r = uuid_xorshift32(&rng_state);
    SREG = oldSREG;

    for (int b = 0; b < 4 && i < len; b++) {
      dest[i++] = (uint8_t)(r & 0xFF);
      r >>= 8;
    }
  }

#elif defined(PLATFORMIO_NATIVE)
  (void)ctx;
  static std::random_device rd;
  static std::mt19937_64 eng(rd());
  static std::uniform_int_distribution<uint8_t> dist(0, 255);
  for (size_t k = 0; k < len; k++) {
    dest[k] = dist(eng);
  }
#else
  (void)ctx;
#ifdef ARDUINO
#warning                                                                       \
    "UUID7: Using Arduino random() fallback. Ensure it is seeded with randomSeed() for uniqueness!"
  for (size_t i = 0; i < len; i++) {
    dest[i] = (uint8_t)(random(256));
  }
#else
// Minimal LCG fallback for environments without a standard RNG source.
#warning                                                                       \
    "UUID7: Using insecure LCG fallback. Not recommended for production use."
  static uint32_t s = 0x12345678U;
  for (size_t i = 0; i < len; ++i) {
    s = 1664525U * s + 1013904223U;
    dest[i] = (uint8_t)(s >> 24);
  }
#endif
#endif
}
