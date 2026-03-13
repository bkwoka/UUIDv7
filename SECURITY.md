# Security Policy

## Supported Versions
Only the latest version of the library is supported with security updates.

## Entropy and RNG Limitations
This library is designed for embedded systems, which often lack a Cryptographically Secure Pseudorandom Number Generator (CSPRNG).

* **ESP32 / ESP8266**: Uses hardware TRNG. Highly secure, provided the radio (Wi-Fi/BT) is enabled or the bootloader RNG is active. *(Note for ESP8266: If the device operates with Wi-Fi disabled, e.g., in Modem/Deep Sleep, use `setRandomSource()` or `mixEntropy()` as `os_get_random()` loses entropy quality without the active radio).*
* **RP2040**: Uses the Ring Oscillator (ROSC) for entropy.
* **AVR (Arduino Uno/Nano/Mega)**: **WARNING.** AVR chips do not have hardware RNG. The library falls back to ADC noise and clock jitter. This is **NOT** cryptographically secure. For production AVR environments, you MUST inject a custom RNG using `setRandomSource()` or use `mixEntropy()` with a unique device ID.

## Thread Safety (RTOS & Multi-core)
* **ESP32**: Fully thread-safe using `portMUX`.
* **RP2040**: Fully thread-safe using hardware spinlocks (requires Earle Philhower core).
* **STM32 / Custom FreeRTOS**: You must define `-DUUID7_USE_FREERTOS_CRITICAL` in your build flags to enable FreeRTOS thread safety. Otherwise, it falls back to disabling global interrupts.

## Reporting a Vulnerability
If you discover a security vulnerability, please open an issue or contact the maintainer directly via GitHub.
