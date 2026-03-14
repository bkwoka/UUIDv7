# Changelog

## [1.3.2] - 2026-03-14

### Fixed
- **Concurrency**: Fixed TOCTOU (Time-of-Check to Time-of-Use) data race in `getTimestamp()` by taking a full buffer snapshot under lock.
- **Concurrency**: Fixed data races in `isV7()`, `isV4()`, and `getVariant()` by moving their implementations to `.cpp` and protecting reads with `UUID7Guard`.
- **Concurrency**: Fixed torn reads/writes in `parse()`, `fromBytes()`, and `mixEntropy()` by moving implementations to `.cpp` and protecting them with `UUID7Guard`.
- **Concurrency**: Fixed a race condition in the AVR PRNG fallback when `generate()` is called simultaneously from the main loop and an ISR.
- **Examples**: Fixed a logical flaw in `2_Robust_Persistence.ino` where using `millis()` directly with EEPROM state caused a permanent fallback to UUIDv4 after reboot.
- **Docs**: Added explicit warnings about EEPROM wear when setting `interval_ms=0` in `setStorage()`.
- **Docs**: Added thread-safety warnings for relational operators (`==`, `<`, etc.).
- **Platform**: Added compiler warning for unsupported platforms missing a default clock source.

## [1.3.1] - 2026-03-14

### Fixed
- **Concurrency**: Resolved a potential data race in UUID v4 generation by using a temporary buffer and copying under lock.
- **Concurrency**: Secured the static clock accumulator in `default_now_ms()` with `UUID7Guard`, preventing torn reads and race conditions on multi-core (RP2040) and RTOS (STM32 FreeRTOS) platforms.
- **Robustness**: Secured AVR PRNG stack entropy from LTO (Link Time Optimization) deletion using `volatile` qualifier.
- **Docs**: Corrected thread safety information for `EasyUUID7::data()` and `EasyUUID7::toCharArray()`.
- **Docs**: Updated relational operators description to highlight optimized 128-bit `memcmp` implementation.

### Changed
- **Performance**: Optimized entropy mixing by removing conditional branches (now unconditional XOR), improving deterministic execution time.
- **Entropy**: Added entropy mixing support to UUID v4 generation for improved collision resistance across fleets.

## [1.3.0] - 2026-03-13

### Changed (Under-the-hood)
- **Architecture**: Massive internal refactoring. Eliminated the "God Object" anti-pattern by splitting `UUID7.cpp` into domain-specific modules (`UUID7Codec`, `TimestampState`, `UUID7Persistence`).
- **HAL Separation**: Hardware-specific RNG and Clock implementations are now strictly isolated in `UUID7Rng.cpp` and `UUID7Clock.cpp`, improving maintainability and cross-platform compilation safety.
- **Optimization**: Unified the monotonicity logic for both 8-bit (AVR) and 32/64-bit architectures, eliminating duplicated code and complex preprocessor macros.
- **Compatibility**: 100% backward compatible with 1.2.x public API. Zero behavioral changes.

### Fixed
- **API**: Added `[[nodiscard]]` attribute to `generate()` to enforce error handling of RNG/Clock failures.
- **Concurrency**: Fixed potential deadlock/UB in `setLockCallbacks()` by enforcing that both lock and unlock callbacks must be provided together.
- **Robustness**: Fixed a bug in `EasyUUID7::toCharArray()` where a transient RNG failure would permanently brick the instance cache. It now safely recovers on subsequent calls.
- **Docs**: Added explicit warnings about object slicing/polymorphism and the semantics of copying `UUID7` instances.

## [1.2.1] - 2026-03-13

### Added
- **Docs**: Added comprehensive API Reference for `EasyUUID7` wrapper in README.
- **Examples**: Added Example 7 (`7_STM32_Fleet_Unique_IDs`) demonstrating `mixEntropy()` with STM32 96-bit hardware UID.
- **Maintenance**: Added `.clang-format` and SPDX license headers to all source files.

### Fixed
- **Platform (ESP32)**: `default_now_ms()` now uses the native 64-bit `esp_timer_get_time()` instead of `millis()`, eliminating the 49-day wraparound concern entirely on ESP32.
- **Platform (Arduino)**: `default_now_ms()` now tracks `millis()` overflows in software, extending the counter to 64 bits and preventing clock-regression fallback after 49.7 days of uptime on AVR, ESP8266, STM32, and RP2040.
- **Testing**: Added `test_filter` to `platformio.ini` environments to properly isolate core tests from wrapper tests.

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
