/*
 * EXAMPLE 3: Custom Entropy Strategy (Dependency Injection)
 * 
 * PROBLEM:
 * On AVR (Arduino Uno/Nano), there is NO hardware RNG. The library uses
 * ADC noise and clock jitter, which is "Best Effort" but not cryptographically secure.
 * If Pin A0 is grounded, entropy drops significantly.
 * 
 * SOLUTION:
 * Inject a custom RNG function. This example mimics a better entropy source
 * (e.g., using a Zener diode noise source, TRNG module, or just better ADC mixing).
 */

#include <Arduino.h>
#include "UUID7.h"

// --- CUSTOM RNG IMPLEMENTATION ---
// This function must fill 'dest' with 'len' random bytes.
// It is called by the library whenever it needs fresh randomness.
void my_secure_rng(uint8_t* dest, size_t len, void* ctx) {
    #if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)
        // On ESP32 we have hardware RNG, but for this example, let's just use it
        for (size_t i = 0; i < len; i += 4) {
            uint32_t r = esp_random();
            for (size_t j = 0; j < 4 && (i + j) < len; j++) {
                dest[i + j] = (r >> (j * 8)) & 0xFF;
            }
        }
    #elif defined(ESP8266)
        os_get_random(dest, len);
    #elif defined(ARDUINO_ARCH_RP2040)
        for (size_t i = 0; i < len; i++) {
            dest[i] = (uint8_t)rp2040.hw_rosc_get_randombyte();
        }
    #else
        // AVR / Other: Improved ADC mixing
        for (size_t i = 0; i < len; i++) {
            uint8_t byte_val = 0;
            
            // Mix multiple analog pins for better entropy on AVR
            for (int b = 0; b < 8; b++) {
                // Portable check for pins. A1/A2 are standard on Uno/Nano.
                #if defined(A1) && defined(A2)
                    int noise = analogRead(A0) ^ analogRead(A1) ^ analogRead(A2);
                #else
                    int noise = analogRead(A0);
                #endif
                byte_val = (byte_val << 1) | (noise & 0x01);
                delayMicroseconds(50); // Small jitter delay
            }
            dest[i] = byte_val ^ (micros() & 0xFF);
        }
    #endif
}

// Inject custom RNG via constructor
UUID7 uuid(my_secure_rng, nullptr, nullptr, nullptr);

void setup() {
    Serial.begin(115200);
    Serial.println("\n--- UUIDv7 Custom Entropy ---");
    Serial.println("Using custom RNG source instead of default fallback.");
    
    // Validate RNG health
    if (uuid.generate()) {
        char buf[37];
        uuid.toString(buf, 37);
        Serial.print("Secure UUID: ");
        Serial.println(buf);
    } else {
        Serial.println("Error: RNG Health Check Failed!");
    }
}

void loop() {
    // ...
}
