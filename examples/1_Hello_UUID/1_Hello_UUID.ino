/*
 * EXAMPLE 1: Hello UUIDv7 (Production Basics)
 * 
 * Demonstrates:
 * 1. Basic generation of UUID v7.
 * 2. Error handling (FAIL FAST).
 * 3. Text conversion and parsing.
 * 4. Comparison operators.
 * 
 * Platforms: AVR, ESP8266, ESP32, RP2040
 */

#include <Arduino.h>
#include "UUID7.h"

// Instantiate the generator using default Time and RNG sources.
// On AVR: Uses micros() + ADC noise (ensure A0 is floating!).
// On ESP: Uses hardware RNG + system timer.
// On RP2040: Uses hardware ROSC RNG.
UUID7 uuid;

void setup() {
    Serial.begin(115200);
    while (!Serial);
    Serial.println("\n--- UUIDv7 Production Basics ---");

    // 1. GENERATION WITH ERROR HANDLING
    // Always check the return value! Generation can fail if:
    // - System clock fails (returns 0).
    // - RNG fails (returns all zeros).
    // - Overflow occurs in FAIL_FAST mode.
    if (uuid.generate()) {
        
        // 2. CONVERSION TO STRING
        // Buffer must be >= 37 bytes (36 chars + null terminator).
        // Using static/stack buffer avoids heap fragmentation.
        char uuidStr[37];
        uuid.toString(uuidStr, sizeof(uuidStr));
        
        Serial.print("Generated UUID: ");
        Serial.println(uuidStr);

        // 3. ACCESSING RAW BYTES (Zero-Copy)
        const uint8_t* raw = uuid.data();
        Serial.print("Raw Bytes: ");
        for(int i=0; i<16; i++) {
            if(raw[i] < 0x10) Serial.print('0');
            Serial.print(raw[i], HEX);
        }
        Serial.println();

        // 4. PARSING BACK (Validation)
        uint8_t parsed[16];
        if (UUID7::parseFromString(uuidStr, parsed)) {
            Serial.println("Parsing: OK");
        }
        
        // 5. ADVANCED FORMATTING
        char custom[37];
        uuid.toString(custom, sizeof(custom), true, true); // UPPERCASE
        Serial.print("Uppercase: "); Serial.println(custom);
        
        uuid.toString(custom, sizeof(custom), false, false); // RAW (no dashes)
        Serial.print("Raw Hex:   "); Serial.println(custom);

    } else {
        Serial.println("CRITICAL: Failed to generate UUID (RNG/Clock Error)!");
        // In production, you might want to retry or enter safe mode.
    }
}

void loop() {
    // 5. MONOTONICITY CHECK LOOP
    static UUID7 prev_uuid;
    static bool first_run = true;

    if (uuid.generate()) {
        if (!first_run) {
            // Operator < compares 128-bit values
            if (prev_uuid < uuid) {
                // This is expected behavior for v7
            } else {
                Serial.println("VIOLATION: Monotonicity lost!");
            }
        }
        
        // Save for next comparison (uses optimized internal memcpy)
        prev_uuid = uuid; 
        first_run = false;
        
        char buf[37];
        uuid.toString(buf, sizeof(buf));
        Serial.println(buf);
    }
    
    delay(1000);
}
