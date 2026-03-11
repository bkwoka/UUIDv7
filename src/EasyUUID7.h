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

/**
 * @class EasyUUID7
 * @brief High-level wrapper for UUIDv7 with automatic string caching and retry logic.
 */
class EasyUUID7 final : public UUID7 {
private:
    char _cacheBuffer[37]; // String representation cache
    uint16_t _max_retries; // Maximum generation attempts before failure

public:
    explicit EasyUUID7(uint16_t max_retries = 100) : UUID7(), _max_retries(max_retries) {
        memset(_cacheBuffer, 0, sizeof(_cacheBuffer));
    }

    /**
     * @brief Generates a new UUID. 
     * BLOCKING CALL: Will retry until successful or max_retries is reached.
     * Automatically updates the internal string cache.
     * @return true if successful, false if max_retries exceeded (e.g., hardware failure).
     */
    bool generate() {
        uint16_t tries = 0;
        
        // Spin-wait until successful or limit reached.
        while (!UUID7::generate()) {
            if (++tries >= _max_retries) {
                return false; // Prevent infinite loop in case of recurrent hardware failure
            }
            yield(); 
        }
        
        // Update cache immediately
        UUID7::toString(_cacheBuffer, sizeof(_cacheBuffer));
        return true;
    }


    /**
     * @brief Parse a 36-character UUID string and update internal cache.
     * @param str36 Source string.
     * @return true if string is valid and parsed, false otherwise.
     */
    bool parse(const char* str36) noexcept {
        bool ok = UUID7::parse(str36);
        if (ok) {
            UUID7::toString(_cacheBuffer, sizeof(_cacheBuffer));
        }
        return ok;
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

