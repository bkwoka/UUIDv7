#include "UUID7.h"
#include <string.h>

// --- PLATFORM INCLUDES & CRITICAL SECTION GUARD ---
#if defined(ARDUINO)
    #include <Arduino.h>
#endif

// Forward declaration of platform specific globals if needed
#if defined(PLATFORMIO_ESP32) || defined(ARDUINO_ARCH_ESP32)
    #include "esp_system.h"
    #include "esp_timer.h"
    // Thread safety guard for ESP32 multi-core environments
    static portMUX_TYPE _uuid_spinlock = portMUX_INITIALIZER_UNLOCKED;
#elif defined(PLATFORMIO_ESP8266) || defined(ESP8266)
    extern "C" {
      #include "user_interface.h" 
    }
#elif defined(ARDUINO_ARCH_AVR) || defined(__AVR__)
    #include <util/atomic.h>
#elif defined(ARDUINO_ARCH_RP2040)
    // Support for Earle Philhower's Core (pico-sdk)
    #include <hardware/structs/rosc.h>
#elif defined(ARDUINO_ARCH_STM32)
    // STM32 HAL is integrated via the Arduino selection.
#elif defined(PLATFORMIO_NATIVE)
    #include <mutex>
    #include <chrono>
    #include <random>
    static std::mutex _uuid_mutex;
#endif

// RAII Guard for Critical Sections
class UUID7Guard {
public:
    UUID7Guard() {
        #if defined(PLATFORMIO_ESP32) || defined(ARDUINO_ARCH_ESP32)
            portENTER_CRITICAL_SAFE(&_uuid_spinlock);
        #elif defined(ARDUINO_ARCH_AVR) || defined(__AVR__)
            _sreg = SREG;
            cli();
        #elif defined(PLATFORMIO_ESP8266) || defined(ESP8266) || defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_STM32)
            noInterrupts();
        #elif defined(PLATFORMIO_NATIVE)
            _uuid_mutex.lock();
        #endif
    }

    ~UUID7Guard() {
        #if defined(PLATFORMIO_ESP32) || defined(ARDUINO_ARCH_ESP32)
            portEXIT_CRITICAL_SAFE(&_uuid_spinlock);
        #elif defined(ARDUINO_ARCH_AVR) || defined(__AVR__)
            SREG = _sreg;
        #elif defined(PLATFORMIO_ESP8266) || defined(ESP8266) || defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_STM32)
            interrupts();
        #elif defined(PLATFORMIO_NATIVE)
            _uuid_mutex.unlock();
        #endif
    }

    // Disable copying
    UUID7Guard(const UUID7Guard&) = delete;
    UUID7Guard& operator=(const UUID7Guard&) = delete;

private:
    #if defined(ARDUINO_ARCH_AVR) || defined(__AVR__)
        uint8_t _sreg;
    #endif
};

// --- CONFIGURATION MACROS ---
#if defined(ARDUINO_ARCH_AVR) || defined(__AVR__)
    #ifndef UUID7_NO_ANALOG_ENTROPY
        #ifndef UUID7_ENTROPY_ANALOG_PIN
            #define UUID7_ENTROPY_ANALOG_PIN A0
        #endif
    #endif
#endif

// --- INTERNAL HELPERS ---
#if defined(ARDUINO_ARCH_AVR) || defined(__AVR__)
static inline uint32_t uuid_mix32(uint32_t k) {
    k ^= k >> 16;
    k *= 0x85ebca6b;
    k ^= k >> 13;
    k *= 0xc2b2ae35;
    k ^= k >> 16;
    return k;
}
static uint32_t uuid_xorshift32(uint32_t* state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}
#endif

// --- CLASS IMPLEMENTATION ---

UUID7::UUID7(fill_random_fn rng, void* rng_ctx, now_ms_fn now, void* now_ctx) noexcept
    : _version(UUID_VERSION_7), 
      _overflowPolicy(UUID_OVERFLOW_FAIL_FAST), 
      _rng(rng), _rng_ctx(rng_ctx), _now(now), _now_ctx(now_ctx), 
#ifndef UUID7_OPTIMIZE_SIZE
      _last_ts_ms(0), 
#endif
      _load(nullptr), _save(nullptr), _storage_ctx(nullptr), 
      _save_interval_ms(10000), _last_saved_ts_ms(0)
{
    memset(_b, 0, sizeof(_b));
#ifdef UUID7_OPTIMIZE_SIZE
    memset(_last_ts_48, 0, 6);
#endif
}

void UUID7::setVersion(UUIDVersion v) {
    _version = v;
}

void UUID7::setStorage(uuid_load_fn load_fn, uuid_save_fn save_fn, void* ctx, uint32_t auto_save_interval_ms) {
    _load = load_fn;
    _save = save_fn;
    _storage_ctx = ctx;
    _save_interval_ms = auto_save_interval_ms;
}

void UUID7::load() {
    if (_load) {
        uint64_t saved_ts = _load(_storage_ctx);
        if (saved_ts > 0) {
            _last_saved_ts_ms = saved_ts;
            uint64_t target = saved_ts + _save_interval_ms;
            
#ifdef UUID7_OPTIMIZE_SIZE
            _last_ts_48[5] = target & 0xFF; target >>= 8;
            _last_ts_48[4] = target & 0xFF; target >>= 8;
            _last_ts_48[3] = target & 0xFF; target >>= 8;
            _last_ts_48[2] = target & 0xFF; target >>= 8;
            _last_ts_48[1] = target & 0xFF; target >>= 8;
            _last_ts_48[0] = target & 0xFF;
#else
            _last_ts_ms = target;
#endif
        }
    }
}

void UUID7::default_fill_random(uint8_t* dest, size_t len, void* ctx) noexcept {
    (void)ctx;

#if defined(PLATFORMIO_ESP32) || defined(ARDUINO_ARCH_ESP32)
    // ESP32 Hardware RNG
    size_t i = 0;
    while (i + 4 <= len) {
        uint32_t r = esp_random();
        memcpy(dest + i, &r, 4);
        i += 4;
    }
    if (i < len) {
        uint32_t r = esp_random();
        while (i < len) { dest[i++] = (uint8_t)(r & 0xFF); r >>= 8; }
    }

#elif defined(PLATFORMIO_ESP8266) || defined(ESP8266)
    // ESP8266 Hardware RNG (WDEV)
    os_get_random((unsigned char*)dest, len);

#elif defined(ARDUINO_ARCH_RP2040)
    // RP2040 Hardware ROSC (Ring Oscillator)
    // Supported on both Earle Philhower and MBED cores.
    // Provides high-entropy jitter but requires multiple clock cycles per bit.
    for (size_t i = 0; i < len; i++) {
        uint8_t r = 0;
        for (int bit = 0; bit < 8; bit++) {
            r = (r << 1) | (rosc_hw->randombit & 1);
        }
        dest[i] = r;
    }

#elif defined(ARDUINO_ARCH_STM32)
    /* 
     * STM32 Entropy Strategy:
     * Combines multiple hardware/temporal sources to maximize uniqueness 
     * even on devices lacking a dedicated TRNG peripheral.
     */
    
    // 1. Initial platform-provided randomness
    for (size_t i = 0; i < len; i++) {
        dest[i] = (uint8_t)random(256);
    }

    // 2. Hardware-Unique Identification (UID Mix)
    // Incorporates the 96-bit Unique Device ID to ensure spatial uniqueness.
    uint32_t uid[3];
    uid[0] = HAL_GetUIDw0();
    uid[1] = HAL_GetUIDw1();
    uid[2] = HAL_GetUIDw2();
    
    for (size_t i = 0; i < len; i++) {
        dest[i] ^= (uint8_t)(uid[i % 3] >> ((i % 4) * 8));
    }

    // 3. Jitter Mix: Temporal entropy from high-resolution SysTick value
    uint32_t tick = SysTick->VAL;
    dest[0] ^= (uint8_t)(tick & 0xFF);
    dest[len-1] ^= (uint8_t)((tick >> 8) & 0xFF);

#elif defined(ARDUINO_ARCH_AVR) || defined(__AVR__)
    #warning "UUID7: Using fallback entropy (ADC noise + Clock Jitter). Not cryptographically secure."

    static uint32_t rng_state = 0;
    
    if (rng_state == 0) {
        uint32_t entropy = 0;
        uint8_t stack_var;
        entropy ^= uuid_mix32((uint16_t)(uintptr_t)&stack_var);
        entropy ^= uuid_mix32((uint32_t)((uintptr_t)default_fill_random));
        entropy ^= uuid_mix32(micros());

        #ifdef UUID7_ENTROPY_ANALOG_PIN
            for(int i = 0; i < 4; i++) { (void)analogRead(UUID7_ENTROPY_ANALOG_PIN); }
            for (int i = 0; i < 8; i++) {
                unsigned long t_start = micros();
                uint16_t val = analogRead(UUID7_ENTROPY_ANALOG_PIN);
                unsigned long t_end = micros();
                entropy = uuid_mix32(entropy ^ val);
                entropy = uuid_mix32(entropy ^ (t_end - t_start));
                delayMicroseconds(10 + (val & 0x0F)); 
            }
        #endif

        for (int i = 0; i < 4; i++) {
            unsigned long t1 = micros();
            for (volatile uint8_t k = 0; k < (uint8_t)((t1 & 0x07) * 10); k++) { __asm__("nop"); }
            unsigned long t2 = micros();
            entropy ^= uuid_mix32(t2 ^ t1);
        }

        if (entropy == 0) entropy = 0xBADC0FFE;
        rng_state = entropy;
    }

    size_t i = 0;
    while (i < len) {
        rng_state ^= (uint32_t)micros();
        uint32_t r = uuid_xorshift32(&rng_state);
        for (int b = 0; b < 4 && i < len; b++) {
            dest[i++] = (uint8_t)(r & 0xFF);
            r >>= 8;
        }
    }

#elif defined(PLATFORMIO_NATIVE)
    static std::random_device rd;
    static std::mt19937_64 eng(rd());
    static std::uniform_int_distribution<uint8_t> dist(0, 255);
    for (size_t k = 0; k < len; k++) {
        dest[k] = dist(eng);
    }
#else
    #ifdef ARDUINO
        #warning "UUID7: Using Arduino random() fallback. Ensure it is seeded with randomSeed() for uniqueness!"
        for (size_t i = 0; i < len; i++) {
            dest[i] = (uint8_t)(random(256));
        }
    #else
        // Minimal LCG fallback for environments without a standard RNG source.
        #warning "UUID7: Using insecure LCG fallback. Not recommended for production use."
        static uint32_t s = 0x12345678U;
        for (size_t i = 0; i < len; ++i) {
            s = 1664525U * s + 1013904223U;
            dest[i] = (uint8_t)(s >> 24);
        }
    #endif
#endif
}

uint64_t UUID7::default_now_ms(void* ctx) noexcept {
    (void)ctx;
#if defined(PLATFORMIO_NATIVE)
    using namespace std::chrono;
    return (uint64_t)duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
#elif defined(ARDUINO)
    return (uint64_t)millis();
#else
    return 0; 
#endif
}

bool UUID7::_nextRandom() noexcept {
    // Increment 74-bit random field (bytes 15 down to 6).
    // Respects version (v7) and variant (RFC) bit masking.
    for (int i = 15; i >= 9; i--) {
        if (++_b[i] != 0) return false;
    }
    
    uint8_t b8 = _b[8] & 0x3F;
    b8++;
    _b[8] = (_b[8] & 0xC0) | (b8 & 0x3F);
    if ((b8 & 0x40) == 0) return false;

    if (++_b[7] != 0) return false;

    uint8_t b6 = _b[6] & 0x0F;
    b6++;
    _b[6] = (_b[6] & 0xF0) | (b6 & 0x0F);
    
    if ((b6 & 0x10) == 0) return false;

    return true; 
}

bool UUID7::generate() {
    fill_random_fn rng = _rng ? _rng : &UUID7::default_fill_random;
    if (!rng) return false;

    if (_version == UUID_VERSION_4) {
        rng(_b, sizeof(_b), _rng_ctx);
        uint8_t sum = 0;
        for (size_t i = 0; i < sizeof(_b); i++) sum |= _b[i];
        if (sum == 0) return false;

        _b[6] = (_b[6] & 0x0F) | 0x40; 
        _b[8] = (_b[8] & 0x3F) | 0x80; 
        return true;
    }

    now_ms_fn now_func = _now ? _now : &UUID7::default_now_ms;
    bool overflow_state = false;

    while (true) {
        uint8_t temp_rand[16];
        rng(temp_rand, sizeof(temp_rand), _rng_ctx);

        uint8_t sum = 0;
        for (size_t i = 0; i < sizeof(temp_rand); i++) sum |= temp_rand[i];
        if (sum == 0) return false; 

        bool save_needed = false;
        uint64_t ts_to_save = 0;
        bool success = false;
        bool overflow_occurred = false;

        {
            UUID7Guard lock; // Ensure thread-safe access to monotonicity state
            uint64_t now_ms = now_func(_now_ctx);
            if (now_ms == 0) return false;

#ifdef UUID7_OPTIMIZE_SIZE
            uint8_t now_48[6];
            uint64_t tmp = now_ms;
            now_48[5] = tmp & 0xFF; tmp >>= 8;
            now_48[4] = tmp & 0xFF; tmp >>= 8;
            now_48[3] = tmp & 0xFF; tmp >>= 8;
            now_48[2] = tmp & 0xFF; tmp >>= 8;
            now_48[1] = tmp & 0xFF; tmp >>= 8;
            now_48[0] = tmp & 0xFF;

            int cmp = memcmp(now_48, _last_ts_48, 6);
            bool major_regression = false;
            
            if (cmp < 0) {
                uint64_t last_ts = 0;
                for(int i=0; i<6; i++) last_ts = (last_ts << 8) | _last_ts_48[i];
                if (now_ms + UUID7_REGRESSION_THRESHOLD_MS < last_ts) {
                    major_regression = true;
                }
            }

            if (major_regression) {
                _version = UUID_VERSION_4;
                memcpy(_b, temp_rand, 16);
                _b[6] = (_b[6] & 0x0F) | 0x40; 
                _b[8] = (_b[8] & 0x3F) | 0x80; 
                return true;
            }

            if (cmp > 0) {
                memcpy(_last_ts_48, now_48, 6);
                memcpy(_b, temp_rand, 16);
                success = true;
                overflow_state = false;
            } else {
                if (overflow_state) {
                    overflow_occurred = true;
                } else {
                    bool initialized = (_b[6] & 0xF0) == 0x70;
                    if (!initialized) {
                        memcpy(_b, temp_rand, 16);
                        success = true;
                    } else {
                        if (_nextRandom()) {
                            overflow_occurred = true; 
                            overflow_state = true; 
                        } else {
                            success = true;
                        }
                    }
                }
            }
            if (success) {
                memcpy(_b, _last_ts_48, 6);
            }
#else
            if (now_ms + UUID7_REGRESSION_THRESHOLD_MS < _last_ts_ms) {
                _version = UUID_VERSION_4;
                memcpy(_b, temp_rand, 16);
                _b[6] = (_b[6] & 0x0F) | 0x40; 
                _b[8] = (_b[8] & 0x3F) | 0x80; 
                return true;
            }

            if (now_ms > _last_ts_ms) {
                _last_ts_ms = now_ms;
                memcpy(_b, temp_rand, 16);
                success = true;
                overflow_state = false;
            } else {
                now_ms = _last_ts_ms; 
                if (overflow_state) {
                    overflow_occurred = true;
                } else {
                    bool initialized = (_b[6] & 0xF0) == 0x70;
                    if (!initialized) {
                        memcpy(_b, temp_rand, 16);
                        success = true;
                    } else {
                        if (_nextRandom()) {
                            overflow_occurred = true;
                            overflow_state = true;
                        } else {
                            success = true;
                        }
                    }
                }
            }
            if (success) {
                uint64_t ts_copy = now_ms;
                ts_copy &= 0x0000FFFFFFFFFFFFULL;
                _b[5] = (uint8_t)(ts_copy & 0xFF); ts_copy >>= 8;
                _b[4] = (uint8_t)(ts_copy & 0xFF); ts_copy >>= 8;
                _b[3] = (uint8_t)(ts_copy & 0xFF); ts_copy >>= 8;
                _b[2] = (uint8_t)(ts_copy & 0xFF); ts_copy >>= 8;
                _b[1] = (uint8_t)(ts_copy & 0xFF); ts_copy >>= 8;
                _b[0] = (uint8_t)(ts_copy & 0xFF);
            }
#endif
            if (success && _save && (now_ms > _last_saved_ts_ms + _save_interval_ms)) {
                save_needed = true;
                _last_saved_ts_ms = now_ms;
                ts_to_save = now_ms;
            }

            if (success) {
                _b[6] = (_b[6] & 0x0F) | ((uint8_t)_version << 4);
                _b[8] = (_b[8] & 0x3F) | 0x80;
            }
        } 

        if (success) {
            if (save_needed && _save) {
                _save(ts_to_save, _storage_ctx);
            }
            return true;
        }

        if (overflow_occurred) {
            if (_overflowPolicy == UUID_OVERFLOW_FAIL_FAST) {
                return false;
            } else {
                #if defined(ARDUINO) || defined(PLATFORMIO_ESP32) || defined(ARDUINO_ARCH_ESP32) || defined(PLATFORMIO_ESP8266) || defined(ESP8266)
                    yield(); 
                #endif
                continue;
            }
        }
        return false;
    }
}

bool UUID7::toString(char* out, size_t buflen, bool uppercase, bool dashes) const noexcept {
    size_t required = dashes ? 37 : 33;
    if (!out || buflen < required) return false;
    
    static const char hexLower[] = "0123456789abcdef";
    static const char hexUpper[] = "0123456789ABCDEF";
    const char* hex = uppercase ? hexUpper : hexLower;
    const uint8_t* p = _b;
    char* s = out;

    // Fast hex conversion with optional hyphenation
    for (int i = 0; i < 16; i++) {
        if (dashes) {
            if (i == 4 || i == 6 || i == 8 || i == 10) {
                *s++ = '-';
            }
        }
        *s++ = hex[(*p >> 4) & 0x0F];
        *s++ = hex[*p++ & 0x0F];
    }
    
    *s = '\0';
    return true;
}

bool UUID7::parseFromString(const char* str, uint8_t out[16]) noexcept {
    if (!str || !out) return false;
    
    size_t len = strlen(str);
    bool dashed;
    
    if (len == 36) {
        dashed = true;
    } else if (len == 32) {
        dashed = false;
    } else {
        return false; // Invalid length
    }

    const char* p = str;
    
    auto hexval = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return 10 + c - 'a';
        if (c >= 'A' && c <= 'F') return 10 + c - 'A';
        return -1;
    };

    for (int i = 0; i < 16; i++) {
        // Skip dashes at specific positions (only if format is dashed)
        // UUID structure: 4-2-2-2-6 bytes
        // Byte indices: 4, 6, 8, 10 start new groups
        if (dashed && (i == 4 || i == 6 || i == 8 || i == 10)) {
            if (*p != '-') return false; // Strict format check
            p++;
        }

        int hi = hexval(*p++);
        int lo = hexval(*p++);
        
        if (hi < 0 || lo < 0) return false; // Invalid char
        
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    
    return true;
}
