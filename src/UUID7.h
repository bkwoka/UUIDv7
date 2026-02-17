#pragma once

#define UUID7_LIB_VERSION "1.0.2"

#include <stdint.h>
#include <stddef.h>

#ifndef UUID7_REGRESSION_THRESHOLD_MS
    #define UUID7_REGRESSION_THRESHOLD_MS 10000
#endif

#if !defined(ARDUINO)
    #include <ostream>
#endif

enum UUIDVersion {
    UUID_VERSION_4 = 4,
    UUID_VERSION_7 = 7
};

enum UUIDOverflowPolicy {
    UUID_OVERFLOW_FAIL_FAST, // Return false immediately (default)
    UUID_OVERFLOW_WAIT       // Busy-wait for the next millisecond
};

#include <string.h>

// Auto-enable optimization for AVR architecture to save Flash/Cycles
#if (defined(ARDUINO_ARCH_AVR) || defined(__AVR__)) && !defined(UUID7_NO_OPTIMIZE)
    #define UUID7_OPTIMIZE_SIZE
#endif

#if defined(ARDUINO)
    #include <Arduino.h>
#endif

#if defined(ARDUINO)
class UUID7 : public Printable {
#else
class UUID7 {
#endif
public:
    typedef void (*fill_random_fn)(uint8_t* dest, size_t len, void* ctx);
    typedef uint64_t (*now_ms_fn)(void* ctx);
    
    // State Persistence Callbacks
    typedef void (*uuid_save_fn)(uint64_t timestamp, void* ctx);
    typedef uint64_t (*uuid_load_fn)(void* ctx);

    /**
     * @brief Initialize generator with optional custom RNG and Time sources.
     * @param rng Pointer to random fill function (nullptr for default).
     * @param rng_ctx User context for RNG.
     * @param now Pointer to millisecond time function (nullptr for default).
     * @param now_ctx User context for time.
     */
    UUID7(fill_random_fn rng = nullptr, void* rng_ctx = nullptr, now_ms_fn now = nullptr, void* now_ctx = nullptr) noexcept;

    /**
     * @brief Set the UUID version to generate (v4 or v7).
     * @param v UUID version.
     */
    void setVersion(UUIDVersion v);

    /**
     * @brief Get current configured UUID version.
     * @return Current version.
     */
    UUIDVersion getVersion() const { return _version; }

    /**
     * @brief Configure behavior when sub-millisecond counter overflows.
     * @param policy Policy (Wait or Fail Fast).
     */
    void setOverflowPolicy(UUIDOverflowPolicy policy) { _overflowPolicy = policy; }

    /**
     * @brief Get current overflow policy.
     * @return Current policy.
     */
    UUIDOverflowPolicy getOverflowPolicy() const { return _overflowPolicy; }

    /**
     * @brief Configure persistence to handle reboots/clock resets.
     * @param load_fn Function to read uint64_t timestamp from NVS/EEPROM.
     * @param save_fn Function to write uint64_t timestamp to NVS/EEPROM.
     * @param ctx Context for callbacks.
     * @param auto_save_interval_ms How often to write state (Wear Leveling). Default 10000ms.
     */
    void setStorage(uuid_load_fn load_fn, uuid_save_fn save_fn, void* ctx, uint32_t auto_save_interval_ms = 10000);

    /**
     * @brief Load state from storage and apply "Safety Jump".
     * MUST be called in setup() if storage is configured.
     * Sets internal clock to: loaded_ts + auto_save_interval_ms.
     * This prevents collisions for the unsaved time window before a crash.
     */
    void load();

    /**
     * @brief Generate a new UUID based on current version setting.
     * 
     * Thread Safety: Safe for ISR/RTOS if:
     * 1. The provided 'now_ms_fn' is ISR-safe (non-blocking).
     * 2. The provided 'fill_random_fn' is reentrant or thread-safe.
     * 
     * Uses "Optimistic RNG" strategy to minimize critical section duration.
     * 
     * @return true if successful, false if time source is invalid (returns 0).
     */
    bool generate();

    /**
     * @brief Import 16 raw bytes into the UUID object.
     * @param bytes Source 16-byte array.
     */
    void fromBytes(const uint8_t bytes[16]) noexcept {
        memcpy(_b, bytes, 16);
    }

    /**
     * @brief Format UUID as standard string.
     * @param out Destination buffer (must be >= 37 bytes for dashed, >= 33 for raw).
     * @param buflen Length of destination buffer.
     * @param uppercase If true, uses UPPERCASE hex.
     * @param dashes If false, omits hyphens (32-char result).
     * @return true if successful, false if buffer is too small.
     */
    bool toString(char* out, size_t buflen, bool uppercase = false, bool dashes = true) const noexcept;

    /**
     * @brief Access raw 16 bytes of the current UUID.
     * @return Pointer to internal byte array.
     */
    const uint8_t* data() const noexcept { return _b; }

    /**
     * @brief Parse a 36-character UUID string into 16-byte binary format.
     * @param str36 Source string.
     * @param out Destination 16-byte array.
     * @return true if string is valid and parsed, false otherwise.
     */
    static bool parseFromString(const char* str36, uint8_t out[16]) noexcept;

    // --- Default Platform Implementations ---
    static void default_fill_random(uint8_t* dest, size_t len, void* ctx) noexcept;
    static uint64_t default_now_ms(void* ctx) noexcept;

#if defined(ARDUINO)
    size_t printTo(Print &p) const override {
        char buf[37];
        toString(buf, sizeof(buf));
        return p.print(buf);
    }
#endif

    // --- Comparison and Logic Operators ---
    
    /** @brief Check if two UUIDs are identical. */
    bool operator==(const UUID7& other) const { return memcmp(_b, other._b, 16) == 0; }
    
    /** @brief Check if two UUIDs are different. */
    bool operator!=(const UUID7& other) const { return !(*this == other); }
    
    /** @brief Lexicographical comparison for sorting (K-Sortable property). */
    bool operator< (const UUID7& other) const { return memcmp(_b, other._b, 16) < 0; }

#if !defined(ARDUINO)
    /**
     * @brief OStream operator for non-Arduino environments (std::cout support).
     */
    friend std::ostream& operator<<(std::ostream& os, const UUID7& uuid) {
        char buf[37];
        uuid.toString(buf, sizeof(buf));
        os << buf;
        return os;
    }
#endif

private:
    uint8_t _b[16];
    UUIDVersion _version;
    UUIDOverflowPolicy _overflowPolicy;
    fill_random_fn _rng;
    void* _rng_ctx;
    now_ms_fn _now;
    void* _now_ctx;
    
    // State for Monotonicity
#ifdef UUID7_OPTIMIZE_SIZE
    uint8_t _last_ts_48[6]; // 48-bit timestamp stored as Big Endian bytes
#else
    uint64_t _last_ts_ms;
#endif

    // Persistence State
    uuid_load_fn _load;
    uuid_save_fn _save;
    void* _storage_ctx;
    uint32_t _save_interval_ms;
    uint64_t _last_saved_ts_ms;

    // Helper to increment random part (74 bits). Returns true if overflow occurred (fail).
    bool _nextRandom() noexcept;
};
