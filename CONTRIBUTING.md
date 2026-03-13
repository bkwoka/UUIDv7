# Contributing to UUIDv7

First off, thank you for considering contributing! It's people like you that make the open-source community such an amazing place.

## How Can I Contribute?

### Reporting Bugs

This section guides you through submitting a bug report. Following these guidelines helps maintainers and the community understand your report, reproduce the behavior, and find related reports.

- Use the **Bug Report** template.
- Include hardware platform (e.g., ESP32, Arduino Uno), library version, and steps to reproduce.

### Suggesting Enhancements

This section guides you through submitting an enhancement suggestion, including completely new features and minor improvements to existing functionality.

- Use the **Feature Request** template.
- Provide a clear and concise description of the suggested enhancement.

### Pull Requests

- Fork the repository.
- Create a new branch for your changes.
- Ensure functionality is covered by tests in `test/`.
- Run `platformio test -e test` to verify.
- Submit a pull request to the `main` branch.

## Development Environment

To run tests locally, you need PlatformIO installed.

```bash
# Run core library tests
platformio test -e test

# Run AVR-compatibility tests (UUID7_OPTIMIZE_SIZE)
platformio test -e test_avr_compat

# Run EasyUUID7 wrapper tests
platformio test -e test_easy
```

## Code Style

This project uses clang-format to enforce a consistent code style.
Before submitting a pull request, format all changed .cpp and .h files:

```bash
clang-format -i src/UUID7.h src/UUID7.cpp src/EasyUUID7.h
```

A .clang-format configuration file is provided at the root of the repository.
Pull requests with unformatted code may be asked to reformat before merging.

## Code of Conduct

Please be respectful and helpful to others in the community.
