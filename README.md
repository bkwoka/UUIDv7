# UUIDv7 for Embedded Systems

[![Release](https://img.shields.io/github/v/tag/bkwoka/UUIDv7?label=release&color=blue)](https://github.com/bkwoka/UUIDv7/releases)
[![Build Status](https://github.com/bkwoka/UUIDv7/actions/workflows/ci.yml/badge.svg)](https://github.com/bkwoka/UUIDv7/actions)
[![Coverage Status](https://coveralls.io/repos/github/bkwoka/UUIDv7/badge.svg?branch=main)](https://coveralls.io/github/bkwoka/UUIDv7?branch=main)
[![Arduino Library Registry](https://img.shields.io/badge/Arduino_Library-UUIDv7-teal.svg)](https://www.arduino.cc/reference/en/libraries/uuidv7/)
[![PlatformIO Registry](https://img.shields.io/badge/PlatformIO-UUIDv7-orange)](https://registry.platformio.org/libraries/bkwoka/UUIDv7)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

An ultra-lightweight, zero-allocation **UUID Version 7 (RFC 9562)** generator optimized for embedded systems (AVR, ESP8266, ESP32, RP2040) and C++11 environments.

## Features

- **RFC 9562 Compliant**: Generates strictly valid Version 7, Variant 2 UUIDs.
- **Time-Ordered**: UUIDs are sortable by creation time (k-sortable).
- **Zero-Allocation**: No `malloc`, `new`, or `std::string`. Uses stack/static memory only.
- **Fail-Fast Design**: Immediate error reporting on clock failure or RNG exhaustion.
- **Monotonicity**: Handles sub-millisecond generation by incrementing the sequence counter, preventing collisions.
- **Persistence Hooks**: Support for saving/restoring timestamp state (EEPROM/NVS) to prevent regression after reboots.
- **Tiny Footprint**: Optimized for 8-bit microcontrollers.

## Installation

### PlatformIO

Add to your `platformio.ini`:

```ini
lib_deps =
    bkwoka/UUIDv7
```

### Arduino IDE

1. Search for **UUIDv7** in the Library Manager.
2. Click **Install**.

## Quick Start

```cpp
#include <UUID7.h>

UUID7 uuid;

void setup() {
    Serial.begin(115200);

    // Always call generate() before accessing data!
    if (uuid.generate()) {
        // Print directly (implements Printable interface)
        Serial.print("Generated: ");
        Serial.println(uuid);
        // Output: 018d960e-2b77-7f8d-9c34-56789abcdef0
    } else {
        Serial.println("Error: RNG or Clock failure!");
    }
}

void loop() {}
```

## Real-World Usage (Time Source)

**Important:** UUIDv7 relies on UTC time. By default, microcontrollers start counting from 0 (1970-01-01) on every boot.
If you use the default configuration in production across multiple devices, you will generate UUIDs that sort incorrectly (all from 1970) and you risk collisions if the random entropy is low.

**Solution:** Inject a time provider (NTP, RTC, or GPS).

```cpp
// Example: Using a global offset from NTP
uint64_t app_start_time = 1698408000000ULL; // Set this via NTP at boot

uint64_t my_time_provider(void* ctx) {
    // Return: Known Epoch + Time since boot
    return app_start_time + millis();
}

void setup() {
    // ... connect to WiFi, get NTP time ...
    uuid.setTimeProvider(my_time_provider);
}
```

### When to use UUID v4 (Random)?

If your device does not have a reliable clock (no RTC, no Internet access), do not use UUIDv7. Use UUIDv4 instead. UUIDv4 is purely random and does not require a clock.

```cpp
uuid.setVersion(UUID_VERSION_4); // Enable random mode
uuid.generate();
```

## Easy Mode vs Pro Mode

This library provides two classes tailored for different needs:

### 1. `UUID7` (Pro Mode - Default)

- **Best for:** Production, Memory-constrained devices (AVR), High-performance logs.
- **RAM Cost:** ~20 bytes per instance (Zero-allocation).
- **Behavior:** Manual buffer management, Fail-fast generation (returns false on error).

### 2. `EasyUUID7` (Easy Mode - Wrapper)

- **Best for:** Beginners, Prototyping, ESP32/8266, Heavy use of `String`.
- **RAM Cost:** ~60 bytes per instance (Adds internal 37-byte buffer).
- **Behavior:** Returns `String`, `toCharArray()`, Auto-retry on generation.

**Usage:**

```cpp
#include "EasyUUID7.h"
EasyUUID7 uuid;
String s = uuid.toString(); // Works!
```

## Security & Entropy (Critical for AVR)

### ESP32 / ESP8266

Uses hardware TRNG (True Random Number Generator). No configuration needed.

### RP2040 (Raspberry Pi Pico)

Uses internal Hardware ROSC (Ring Oscillator) to generate high-quality entropy. No configuration needed.

### AVR (Arduino Uno/Nano)

AVR lacks hardware RNG. By default, this library uses jitter and ADC noise.
**Warning: Without a custom RNG source, AVR entropy is based on ADC noise and is not cryptographically secure.**
**Recommendation:** Leave pin `A0` floating or connect a noise source.
To use a different pin (e.g., A3):

```cpp
// In platformio.ini or before include
#define UUID7_ENTROPY_ANALOG_PIN A3
#include <UUID7.h>
```

**Production Security:**
For high-security applications on AVR, inject a custom CSPRNG source:

```cpp
void my_secure_rng(uint8_t* dest, size_t len, void* ctx) {
    // Read from ATECC608 or similar secure element
}
UUID7 uuid(my_secure_rng);
```

## Clock Regression & Security

UUIDv7 depends on a monotonic clock. If the clock resets significantly (e.g., after battery failure), the library protects against collisions by switching to Version 4 (random) mode.
The default threshold is 10 seconds, but it can be customized:

```cpp
#define UUID7_REGRESSION_THRESHOLD_MS 1000 // Switch to v4 after 1s regression
#include <UUID7.h>
```

## Native C++ Support

Outside of Arduino, the library supports standard `std::ostream`:

```cpp
#include <iostream>
#include <UUID7.h>

UUID7 uuid;
if (uuid.generate()) {
    std::cout << "Generated: " << uuid << std::endl;
}
```

## Persistence (Safety Jump)

UUIDv7 relies on time. To prevent generating duplicate UUIDs after a device reboot (if the clock resets), use the Storage API:

```cpp
// Load last timestamp from EEPROM on boot
uuid.setStorage(load_fn, save_fn, nullptr);
uuid.load(); // Performs "Safety Jump" ahead of last saved time
```

## Performance

| Platform | Flash Cost | RAM Cost | Time per UUID |
| -------- | ---------- | -------- | ------------- |
| AVR      | ~1.5 KB    | ~80 B    | ~60 µs        |
| ESP32    | ~1.0 KB    | ~60 B    | ~5 µs         |
| RP2040   | ~1.2 KB    | ~60 B    | ~8 µs         |

## API Reference

- `bool generate()`: Generates a new UUID. Returns `false` on failure.
- `const uint8_t* data()`: Access raw 16 bytes.
- `void fromBytes(const uint8_t* bytes)`: Import 16 raw bytes into the object.
- `bool toString(char* out, size_t buflen, bool uppercase = false, bool dashes = true)`: Converts to string. Support for `UPPERCASE` and `raw hex` (no dashes).
- `static bool parseFromString(const char* str, uint8_t* out)`: Validates and parses UUID string.

## License

MIT License. See [LICENSE](LICENSE) file.
