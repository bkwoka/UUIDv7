// SPDX-License-Identifier: MIT
// Copyright (c) 2026 bkwoka
// Repository: https://github.com/bkwoka/UUIDv7

#pragma once

#if defined(ARDUINO)
#include <Arduino.h>
#endif

#if defined(PLATFORMIO_ESP32) || defined(ARDUINO_ARCH_ESP32)
#include "freertos/FreeRTOS.h"
extern portMUX_TYPE _uuid_spinlock;
#elif defined(ARDUINO_ARCH_AVR) || defined(__AVR__)
#include <util/atomic.h>
#elif defined(ARDUINO_ARCH_RP2040) && defined(PICO_SDK_VERSION_MAJOR)
#include <hardware/sync.h>
extern spin_lock_t *_uuid_rp2040_spinlock;
#elif defined(PLATFORMIO_NATIVE)
#include <mutex>
extern std::mutex _uuid_mutex;
#endif

class UUID7Guard {
public:
  inline UUID7Guard(void (*lock_cb)(void), void (*unlock_cb)(void))
      : _lock_cb(lock_cb), _unlock_cb(unlock_cb) {
    if (_lock_cb) {
      _lock_cb();
      return;
    }
#if defined(PLATFORMIO_ESP32) || defined(ARDUINO_ARCH_ESP32)
    portENTER_CRITICAL_SAFE(&_uuid_spinlock);
#elif defined(ARDUINO_ARCH_AVR) || defined(__AVR__)
    _sreg = SREG;
    cli();
#elif defined(ARDUINO_ARCH_RP2040) && defined(PICO_SDK_VERSION_MAJOR)
    if (_uuid_rp2040_spinlock) {
      _saved_irq = spin_lock_blocking(_uuid_rp2040_spinlock);
    } else {
      noInterrupts();
    }
#elif defined(PLATFORMIO_ESP8266) || defined(ESP8266) ||                       \
    defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_STM32)
    noInterrupts();
#elif defined(PLATFORMIO_NATIVE)
    _uuid_mutex.lock();
#endif
  }

  inline ~UUID7Guard() {
    if (_unlock_cb) {
      _unlock_cb();
      return;
    }
#if defined(PLATFORMIO_ESP32) || defined(ARDUINO_ARCH_ESP32)
    portEXIT_CRITICAL_SAFE(&_uuid_spinlock);
#elif defined(ARDUINO_ARCH_AVR) || defined(__AVR__)
    SREG = _sreg;
#elif defined(ARDUINO_ARCH_RP2040) && defined(PICO_SDK_VERSION_MAJOR)
    if (_uuid_rp2040_spinlock) {
      spin_unlock(_uuid_rp2040_spinlock, _saved_irq);
    } else {
      interrupts();
    }
#elif defined(PLATFORMIO_ESP8266) || defined(ESP8266) ||                       \
    defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_STM32)
    interrupts();
#elif defined(PLATFORMIO_NATIVE)
    _uuid_mutex.unlock();
#endif
  }

  UUID7Guard(const UUID7Guard &) = delete;
  UUID7Guard &operator=(const UUID7Guard &) = delete;

private:
  void (*_lock_cb)(void);
  void (*_unlock_cb)(void);
#if defined(ARDUINO_ARCH_AVR) || defined(__AVR__)
  uint8_t _sreg;
#elif defined(ARDUINO_ARCH_RP2040) && defined(PICO_SDK_VERSION_MAJOR)
  uint32_t _saved_irq;
#endif
};
