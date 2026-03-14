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
    return 1773259080000ULL + millis(); // Mock for example
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
*   **STM32**: Uses Arduino `random()` XOR-ed with 96-bit UID and SysTick. **Warning:** Guarantees spatial uniqueness, but is NOT cryptographically secure.
*   **AVR (Uno/Nano)**: Uses ADC noise. **Warning:** Not cryptographically secure by default.

**For AVR Production:**
Connect a noise source to a floating pin or inject a custom RNG:
```cpp
// Use a specific analog pin for entropy (e.g., in setup)
uuid.setEntropyAnalogPin(A3);
```
Or inject a custom CSPRNG function:
```cpp
void my_secure_rng(uint8_t* dest, size_t len, void* ctx) {
    // Read from ATECC608 secure element
}
uuid.setRandomSource(my_secure_rng);
```

**Note for ESP8266:** If the device operates with Wi-Fi disabled (e.g., in Deep Sleep), use `setRandomSource()` as the hardware RNG loses entropy quality without the active radio.

### 2. Clock Regression (v7)
If the clock moves backwards (e.g., NTP adjustment), UUIDv7 protects monotonicity. If the regression is huge (device reset to 1970), it handles it safely.
Custom threshold:
```cpp
// Set threshold to 10 seconds (e.g., in setup)
uuid.setRegressionThreshold(10000);
```

> 💡 **Arduino `millis()` 49-day wraparound:** The default time source automatically
> handles the 32-bit `millis()` overflow (which occurs every ~49.7 days) by tracking
> overflows in software and extending the counter to 64 bits.
> **Note:** This requires `generate()` to be called at least once within any 49-day
> window. If your device spends months in Deep Sleep without waking, inject an RTC or
> NTP provider via `setTimeProvider()`.

### 3. Persistence (Safety Jump)
To prevent generating duplicate UUIDs after a reboot (if the clock isn't perfectly synced), save the state to non-volatile memory:
```cpp
uuid.setStorage(load_fn, save_fn, nullptr);
uuid.load(); // Applies "Safety Jump" on boot
```

---

## Easy Mode vs Pro Mode

### `UUID7` (Pro Mode - Default)
*   **RAM:** ~68 B (ESP32/RP2040/STM32) / ~88 B (AVR) — see Performance table.
*   **Behavior:** Zero-allocation, returns `bool` on success/fail.
*   **Use case:** High-performance logging, tiny AVR devices.

### `EasyUUID7` (Wrapper)
*   **RAM:** ~105 B (ESP32/RP2040/STM32) / ~125 B (AVR) — base class + 37 B string cache.
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
| **AVR (Uno)** | ~1.5 KB | ~88 B | ~60 µs |
| **ESP32** | ~1.0 KB | ~68 B | ~5 µs |
| **RP2040** | ~1.2 KB | ~68 B | ~8 µs |
| **STM32** | ~1.2 KB | ~68 B | ~8 µs |

*Note: RP2040 multi-core spinlocks require Earle Philhower's `pico-sdk` core. Mbed core falls back to global interrupt disable.*

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

### Core Methods
*   `bool generate()`: Generates a new UUID. Returns false on hardware RNG/Clock failure.
*   `void setVersion(UUIDVersion v)`: Set `UUID_VERSION_7` or `UUID_VERSION_4`.
*   `bool toString(char* out, size_t buflen, bool uppercase = false, bool dashes = true)`: Convert to string.
*   `const uint8_t* data()`: Access raw 16 bytes.

### Configuration (Dependency Injection)
*   `void setTimeProvider(now_ms_fn now, void* ctx)`: **Required for v7.** Inject time source.
*   `void setRandomSource(fill_random_fn rng, void* ctx)`: Inject custom RNG.
*   `void mixEntropy(uint64_t seed)`: Inject additional entropy (e.g., MAC address) to prevent collisions across fleets without NTP.
*   `void setOverflowPolicy(UUIDOverflowPolicy policy)`: Set behavior for sub-millisecond overflow (`FAIL_FAST` or `WAIT`).
*   `void setRegressionThreshold(uint32_t ms)`: Set custom threshold for clock regression.
*   `void setEntropyAnalogPin(int16_t pin)`: Set analog pin for AVR entropy generation.
*   `void setLockCallbacks(lock_fn_t lock, lock_fn_t unlock)`: Inject custom thread locks (e.g., FreeRTOS).

### Parsing & Inspection
*   `bool parse(const char* str36)`: Parse a string directly into the instance.
*   `static bool parseFromString(const char* str36, uint8_t out[16])`: Parse string to binary.
*   `void fromBytes(const uint8_t bytes[16])`: Import raw bytes.
*   `UUIDVersion getVersion()`: Returns the version of the current UUID.
*   `uint8_t getVariant()`: Returns the variant (should be 2 for RFC 4122).
*   `bool isV7() / bool isV4()`: Quick version checks.
*   `bool isValid() const`: Check if the object contains a valid, generated UUID.
*   `uint64_t getTimestamp() const`: Extract the 48-bit timestamp (returns 0 if not v7).

### Relational Operators
*   `==`, `!=`, `<`, `>`, `<=`, `>=`: Fully supported for K-Sortable database indexing (uses highly optimized 128-bit `memcmp` under the hood).

---

## EasyUUID7 API Reference

`EasyUUID7` extends `UUID7` with automatic string caching and Arduino `String` integration.
Include with `#include "EasyUUID7.h"`.

> **RAM overhead:** +37 B for the internal string cache compared to `UUID7`.

### Constructor
*   `EasyUUID7(uint16_t max_retries = 100)`: Creates the wrapper. `max_retries` limits spin-wait
    iterations when hardware RNG fails — prevents WDT resets.

### Generation & String Access
*   `bool generate()`: Generates a UUID with automatic retry. Updates internal string cache.
    Returns `false` only if `max_retries` is exhausted (hardware failure).
*   `char* toCharArray()`: Returns a pointer to the internal 37-byte C-string cache.
    **Lazy:** triggers `generate()` on first call if the buffer is empty.
    On critical failure returns the Nil UUID string `"00000000-0000-0000-0000-000000000000"`.
*   `String toString(bool uppercase = false, bool dashes = true)`: Returns an Arduino `String`.
    Does **not** use the internal cache — builds a new `String` each call.

### Parsing
*   `bool parse(const char* str36)`: Parses a UUID string and synchronizes the internal cache.
*   `bool parse(const String& str)`: Overload accepting Arduino `String`.
*   `void fromBytes(const uint8_t bytes[16])`: Imports raw bytes and updates the cache.

### Convenience Operators
*   `operator String() const`: Allows `String s = uuid;`
*   `operator const char*()`: Allows `const char* s = uuid;` (non-const, triggers lazy init).
*   `Serial.println(uuid)`: Via inherited `Printable` interface.

> **Note:** When sharing a single instance across threads (ESP32/RP2040), `data()` and 
> `toCharArray()` are **not thread-safe** as they return pointers to internal buffers that 
> may be overwritten concurrently. Use `toString()` instead, which safely copies the data 
> under an internal spinlock.

## License

MIT License. See [LICENSE](https://opensource.org/licenses/MIT) file.
