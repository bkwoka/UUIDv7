# Changelog
 
## [1.2.0] - 2026-03-13

### Added
- **API**: Added `getTimestamp()` to extract 48-bit Unix timestamp from v7 UUIDs.
- **API**: Added `isValid()` for easy object state verification.
- **API**: Added `setRegressionThreshold()` runtime method, replacing the `UUID7_REGRESSION_THRESHOLD_MS` compile-time macro (fixes Arduino IDE compatibility).
- **API**: Added `setEntropyAnalogPin()` runtime method, replacing the `UUID7_ENTROPY_ANALOG_PIN` compile-time macro (fixes Arduino IDE compatibility).
- **Concurrency**: Added `setLockCallbacks()` for injecting custom RTOS mutexes (e.g., FreeRTOS/STM32), replacing the `UUID7_USE_FREERTOS_CRITICAL` compile-time macro.
- **Testing**: Added `test_avr_compat` environment for native verification of `UUID7_OPTIMIZE_SIZE` builds.

### Fixed
- **Correctness**: `isV7()` and `isV4()` now inspect buffered version bits, providing accurate state after clock regression.
- **Robustness**: `EasyUUID7::toCharArray()` now safely returns Nil UUID string ("00000000-...") on critical RNG failures.
- **Platform (RP2040)**: Updated example 3 to use `rp2040.hwrand32()` for more reliable hardware entropy.
- **Keywords**: Added missing methods to `keywords.txt` for Arduino IDE syntax highlighting.

### Changed
- **CI/CD**: Modernized GitHub Actions with Reusable Workflows and automated GitHub Releases.
- **CI/CD**: Updated CI environment to Python 3.12 and Actions Cache v4.
 

## [1.1.1] - 2026-03-13

### Fixed
- **Concurrency**: Fixed potential deadlock when using blocking time providers (e.g., I2C RTC) by implementing an Optimistic Read strategy.
- **Thread Safety**: Fixed torn reads in `toString()` in multi-core environments by adding a local snapshot under spinlock.
- **RTOS Stability**: Replaced `yield()` with `delay(1)` in `UUID_OVERFLOW_WAIT` policy to prevent Task Watchdog (TWDT) resets on ESP32/FreeRTOS.
- **Persistence**: Fixed ping-pong EEPROM overwrite logic in `2_Robust_Persistence` example to correctly repair corrupted slots.
- **API**: Fixed `EasyUUID7` implicit casting to prevent returning empty strings on uninitialized const objects.
- **Bugfix**: Restored `initialized` check during same-millisecond generation to prevent low-entropy UUIDs immediately after a state load.

## [1.1.0] - 2026-03-11

### Added
- **Entropy Mixing (`mixEntropy`)**: Inject custom entropy (e.g., MAC address hash) to guarantee uniqueness across fleets without NTP sync.
- **Thread Safety**: Hardware spinlock support for RP2040 (Earle Philhower core) and opt-in FreeRTOS critical sections (`UUID7_USE_FREERTOS_CRITICAL`).
- **New API Methods**: `getVersion()`, `getVariant()`, `isV7()`, `isV4()`, and instance-level `parse()`.
- **Relational Operators**: Added `>`, `>=`, `<=`.
- **EasyUUID7 Escape Hatch**: Added `max_retries` to prevent WDT resets on hardware RNG failure.

### Changed
- **[Bug Fix / Behavior Change]**: Clock regression (time moving backwards) now correctly generates a fallback UUIDv4 *only* for the current call, without permanently altering the object's configured version.
- Renamed `setRandomProvider` to `setRandomSource` (old method kept as `[[deprecated]]` for backward compatibility).
- Internal `_nextRandom` logic inverted to `_incrementRandom` for better readability.

### Fixed
- Fixed 48-bit timestamp masking in `OPTIMIZE_SIZE` (AVR) branch to strictly comply with RFC 9562.
- Fixed potential stale state in `EasyUUID7` when parsing strings.

## [1.0.0] - 2026-02-01

### Added
- Initial implementation of UUID Version 7 (RFC 9562) for Arduino/embedded systems.
- Zero-allocation design: no `malloc`, no `new`, no `std::string`.
- Platform-specific entropy sources: ESP32 hardware TRNG, ESP8266 WDEV RNG, AVR ADC noise fallback.
- Monotonicity guarantee via sub-millisecond counter increment.
- Fail-fast API: `generate()` returns `bool` on RNG or clock failure.
- `toString()` / `parseFromString()` with 36-char standard format.
- `setTimeProvider()` / `setRandomSource()` for dependency injection.
- Persistence hooks: `setStorage()` / `load()` with Safety Jump mechanism.
- Overflow policy: `UUID_OVERFLOW_FAIL_FAST` / `UUID_OVERFLOW_WAIT`.
- Native C++11 Linux/macOS support for testing.
- PlatformIO and Arduino Library Manager packaging.
