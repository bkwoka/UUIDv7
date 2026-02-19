# UUID v7 & v4 for Embedded Systems

[![Release](https://img.shields.io/github/v/tag/bkwoka/UUIDv7?label=release&color=blue)](https://github.com/bkwoka/UUIDv7/releases)
[![Build Status](https://github.com/bkwoka/UUIDv7/actions/workflows/ci.yml/badge.svg)](https://github.com/bkwoka/UUIDv7/actions)
[![Coverage Status](https://coveralls.io/repos/github/bkwoka/UUIDv7/badge.svg?branch=main)](https://coveralls.io/github/bkwoka/UUIDv7?branch=main)
[![Arduino Library Registry](https://img.shields.io/badge/Arduino_Library-UUIDv7-teal.svg)](https://www.arduino.cc/reference/en/libraries/uuidv7/)
[![PlatformIO Registry](https://img.shields.io/badge/PlatformIO-UUIDv7-orange)](https://registry.platformio.org/libraries/bkwoka/UUIDv7)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

An ultra-lightweight, zero-allocation UUID generator optimized for embedded systems (AVR, ESP8266, ESP32, RP2040) and C++11 environments.

## Supported UUID Versions

This library implements two specific versions of the UUID standard to cover all embedded use cases:

1.  **UUID Version 7 (RFC 9562)** – Time-ordered, k-sortable. **(Default)**
2.  **UUID Version 4 (RFC 4122)** – Fully random, no clock required.

## ⚡ Which version should I choose?

Embedded devices often lack a Real-Time Clock (RTC). Using the wrong version can lead to collisions or incorrect sorting.

| Feature | **UUID v7** | **UUID v4** |
| :--- | :--- | :--- |
| **Primary Component** | Timestamp (48-bit) | Randomness (122-bit) |
| **Sortable by Time** | ✅ Yes | ❌ No |
| **Database Indexing** | 🚀 Excellent (Clustered Index) | ⚠️ Poor (Fragmentation) |
| **Requires RTC/NTP** | 🔴 **YES (Critical)** | 🟢 **NO** |
| **Offline Devices** | ❌ Avoid (Resets to 1970) | ✅ **Recommended** |

### Guidelines
*   **Use UUIDv7 if:** You have a reliable Time Source (RTC module, NTP sync, GPS) and need to sort data by creation time.
*   **Use UUIDv4 if:** Your device works offline, has no battery backup for time, or frequently reboots without time sync.

---

## Installation

### PlatformIO
```ini
lib_deps =
    bkwoka/UUIDv7
```

### Arduino IDE
1. Open **Library Manager**.
2. Search for **UUIDv7**.
3. Click **Install**.

---

## Quick Start

### Option A: UUID v7 (Time-Based)
*Best for IoT nodes with NTP/RTC.*

```cpp
#include <UUID7.h>

UUID7 uuid;

// 1. Define a time provider (e.g., NTP or RTC)
uint64_t my_time_provider(void* ctx) {
    // Return Unix Epoch in Milliseconds
    // Example: return rtc.getUnixTime() * 1000 + rtc.getMillis();
    return 1698408000000ULL + millis(); // Mock for example
}

void setup() {
    Serial.begin(115200);

    // 2. Inject the provider (CRITICAL for v7)
    uuid.setTimeProvider(my_time_provider);

    if (uuid.generate()) {
        Serial.println(uuid);
        // Output: 018b7... (Time-ordered)
    }
}

void loop() {}
```

### Option B: UUID v4 (Random)
*Best for simple devices without a clock.*

```cpp
#include <UUID7.h>

UUID7 uuid;

void setup() {
    Serial.begin(115200);

    // 1. Switch to Version 4 (Random Mode)
    uuid.setVersion(UUID_VERSION_4);

    if (uuid.generate()) {
        Serial.println(uuid);
        // Output: 9b2075a5-483d-4920-a2bc-39d2d6452301
    }
}

void loop() {}
```

---

## Features

- **RFC Compliant**: Strict adherence to RFC 9562 (v7) and RFC 4122 (v4).
- **Zero Allocation**: No `malloc`, `new`, or `std::string`. Safe for minimal stack.
- **Fail-Fast**: Reports errors immediately (e.g., if RNG fails).
- **Monotonicity (v7)**: Handles sub-millisecond generation by incrementing the sequence counter.
- **Persistence Hooks**: Save/Restore timestamp state to EEPROM to prevent regression.
- **Tiny Footprint**: Optimized for 8-bit microcontrollers.

---

## Configuration & Security

### 1. Security & Entropy (RNG)
The library automatically selects the best available entropy source:
*   **ESP32 / ESP8266**: Uses hardware TRNG (`esp_random`, `os_get_random`).
*   **RP2040**: Uses hardware ROSC (Ring Oscillator).
*   **AVR (Uno/Nano)**: Uses ADC noise. **Warning:** Not cryptographically secure by default.

**For AVR Production:**
Connect a noise source to a floating pin or inject a custom RNG:
```cpp
// Use a specific analog pin for entropy
#define UUID7_ENTROPY_ANALOG_PIN A3
#include <UUID7.h>
```
Or inject a custom CSPRNG function:
```cpp
void my_secure_rng(uint8_t* dest, size_t len, void* ctx) {
    // Read from ATECC608 secure element
}
uuid.setRandomProvider(my_secure_rng);
```

### 2. Clock Regression (v7)
If the clock moves backwards (e.g., NTP adjustment), UUIDv7 protects monotonicity. If the regression is huge (device reset to 1970), it handles it safely.
Custom threshold:
```cpp
#define UUID7_REGRESSION_THRESHOLD_MS 10000 // 10 seconds
#include <UUID7.h>
```

### 3. Persistence (Safety Jump)
To prevent generating duplicate UUIDs after a reboot (if the clock isn't perfectly synced), save the state to non-volatile memory:
```cpp
uuid.setStorage(load_fn, save_fn, nullptr);
uuid.load(); // Applies "Safety Jump" on boot
```

---

## Easy Mode vs Pro Mode

### `UUID7` (Pro Mode - Default)
*   **RAM:** ~20 bytes
*   **Behavior:** Zero-allocation, returns `bool` on success/fail.
*   **Use case:** High-performance logging, tiny AVR devices.

### `EasyUUID7` (Wrapper)
*   **RAM:** ~60 bytes (Internal buffer)
*   **Behavior:** Returns `String`, auto-retries on collision.
*   **Use case:** Prototyping, ESP32 web servers.

```cpp
#include "EasyUUID7.h"
EasyUUID7 uuid;
String id = uuid.toString();
```

---

## Performance

| Platform | Flash Cost | RAM Cost | Time per UUID |
| :--- | :--- | :--- | :--- |
| **AVR (Uno)** | ~1.5 KB | ~80 B | ~60 µs |
| **ESP32** | ~1.0 KB | ~60 B | ~5 µs |
| **RP2040** | ~1.2 KB | ~60 B | ~8 µs |

---

## Native C++ Support

Works in standard C++11 environments (Linux, macOS, Windows) for testing or CLI tools.

```cpp
#include <iostream>
#include <UUID7.h>

UUID7 uuid;

int main() {
    if (uuid.generate()) {
        std::cout << uuid << std::endl;
    }
}
```

## API Reference

*   `bool generate()`: Generates a new UUID.
*   `void setVersion(UUIDVersion v)`: Set `UUID_VERSION_7` or `UUID_VERSION_4`.
*   `void setTimeProvider(now_ms_fn now, void* ctx)`: **Required for v7.**
*   `void setRandomProvider(fill_random_fn rng, void* ctx)`: Inject custom RNG.
*   `bool toString(char* out, ...)`: Convert to string.
*   `static bool parseFromString(...)`: Parse string to binary.
*   `const uint8_t* data()`: Access raw bytes.

## License

MIT License. See [LICENSE](LICENSE) file.
