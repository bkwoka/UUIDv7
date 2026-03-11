# Changelog

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
