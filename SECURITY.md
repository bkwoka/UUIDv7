# Security Policy

## Supported Versions
Only the latest version of the library is supported with security updates.

## Entropy and RNG Limitations
This library is designed for embedded systems, which often lack a Cryptographically Secure Pseudorandom Number Generator (CSPRNG).

* **ESP32 / ESP8266**: Uses hardware TRNG. Highly secure, provided the radio (Wi-Fi/BT) is enabled or the bootloader RNG is active. *(Note for ESP8266: If the device operates with Wi-Fi disabled, e.g., in Modem/Deep Sleep, use `setRandomSource()` or `mixEntropy()` as `os_get_random()` loses entropy quality without the active radio).*
* **RP2040**: Uses the Ring Oscillator (ROSC) for entropy.
* **STM32**: **WARNING.** The default implementation uses the Arduino LCG (`random()`) XOR-ed with the 96-bit hardware UID and SysTick jitter. This guarantees *per-device* uniqueness but is **NOT** cryptographically secure. For production, call `randomSeed()` in your setup, or inject a hardware RNG via `setRandomSource()`.
* **AVR (Arduino Uno/Nano/Mega)**: **WARNING.** AVR chips do not have hardware RNG. The library falls back to ADC noise and clock jitter. This is **NOT** cryptographically secure. For production AVR environments, you MUST inject a custom RNG using `setRandomSource()` or use `mixEntropy()` with a unique device ID.

## Thread Safety (RTOS & Multi-core)
* **ESP32**: Fully thread-safe using `portMUX`.
* **RP2040**: Fully thread-safe using hardware spinlocks (requires Earle Philhower core).
* **STM32 / Custom FreeRTOS**: You can inject custom thread-safety locks at runtime using `uuid.setLockCallbacks()`. For example, in FreeRTOS: `uuid.setLockCallbacks([](){ taskENTER_CRITICAL(); }, [](){ taskEXIT_CRITICAL(); });`.

## Reporting a Vulnerability
If you discover a security vulnerability, please use the GitHub Private Security Advisories feature to report it confidentially, or contact the maintainer directly.
