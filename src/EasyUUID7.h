#pragma once

#if defined(ARDUINO)
    #include <Arduino.h>
#else
    #include <string>
    #include <cstring>
    // Minimal String mock for Native tests
    class String : public std::string {
    public:
        using std::string::string;
        String(const std::string& s) : std::string(s) {}
        bool equals(const char* s) const { return *this == s; }
    };
    inline void yield() {}
#endif
#include "UUID7.h"

/*
 * EasyUUID7 - High-Level Wrapper for UUIDv7
 * 
 * FEATURES:
 * - Automatic String conversion (Arduino String class).
 * - Internal buffer caching (toCharArray() returns stable pointer).
 * - Auto-retry on generation failure (spin-lock).
 * 
 * COST:
 * - Increases RAM footprint by ~37 bytes per instance due to internal caching.
 * - String operations may impact heap fragmentation if used frequently in tight loops.
 */
class EasyUUID7 final : public UUID7 {
private:
    char _cacheBuffer[37]; // Internal cache for persistence and char* access

public:
    EasyUUID7() : UUID7() {
        memset(_cacheBuffer, 0, sizeof(_cacheBuffer));
    }

    /**
     * @brief Generates a new UUID. 
     * BLOCKING CALL: Will retry until successful (handles clock/RNG collisions).
     * Automatically updates the internal string cache.
     */
    bool generate() {
        // Spin-wait until successful. 
        // In highly contested environments, this ensures a UUID is always returned.
        while (!UUID7::generate()) {
            yield(); // Yield to background processes (WiFi/Watchdog)
        }
        
        // Update cache immediately
        UUID7::toString(_cacheBuffer, sizeof(_cacheBuffer));
        return true;
    }

    /**
     * @brief Import 16 raw bytes and update cache.
     * @param bytes Source 16-byte array.
     */
    void fromBytes(const uint8_t bytes[16]) noexcept {
        UUID7::fromBytes(bytes);
        UUID7::toString(_cacheBuffer, sizeof(_cacheBuffer));
    }

    /**
     * @brief Returns pointer to internal char buffer.
     * Compatible with legacy C-style APIs.
     */
    char* toCharArray() {
        // Lazy generation if buffer is empty
        if (_cacheBuffer[0] == 0) {
            generate();
        }
        return _cacheBuffer;
    }

    /**
     * @brief Returns Arduino String object with optional formatting.
     * @param uppercase If true, uses UPPERCASE hex.
     * @param dashes If false, omits hyphens.
     */
    String toString(bool uppercase = false, bool dashes = true) {
        char buf[37];
        UUID7::toString(buf, sizeof(buf), uppercase, dashes);
        return String(buf);
    }

    // --- CONVENIENCE OPERATORS ---

    // Allows: String s = uuid;
    operator String() {
        return toString();
    }
    
    // Allows: const char* s = uuid;
    operator const char*() {
        return toCharArray();
    }
    
    // Allows: Serial.println(uuid) via Printable interface (inherited)
};
