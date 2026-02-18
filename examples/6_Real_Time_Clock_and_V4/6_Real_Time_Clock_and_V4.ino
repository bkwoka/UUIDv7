/*
 * EXAMPLE 6: Real Time & UUID v4 (Random)
 * 
 * WHY THIS MATTERS:
 * UUIDv7 is time-based. By default, Arduino starts at time 0 (1970-01-01).
 * If you have multiple devices, their UUIDs might collide or sort incorrectly
 * if they all think it's 1970.
 * 
 * SOLUTIONS SHOWN:
 * 1. Injecting a Real Time Clock (RTC) or NTP offset.
 * 2. Using UUIDv4 (Pure Random) when no clock is available.
 */

#include <Arduino.h>
#include "UUID7.h"

UUID7 uuid;

// --- SCENARIO A: REAL TIME (UUIDv7) ---

// In a real app, this would come from an NTP server or DS3231 RTC module.
// Let's simulate a start time: 2023-10-27 12:00:00 UTC (in milliseconds)
uint64_t base_timestamp_ms = 1698408000000ULL; 

// Custom Time Provider Wrapper
// Returns: Base Timestamp + Millis since boot
uint64_t my_ntp_provider(void* ctx) {
    (void)ctx;
    return base_timestamp_ms + millis();
}

void setup() {
    Serial.begin(115200);
    while(!Serial);
    Serial.println("\n--- UUID Real-World Setup ---");

    // 1. CONFIGURE TIME SOURCE
    // Instead of starting at 1970, we inject our provider.
    // Now UUIDs will reflect the real calendar date.
    uuid.setTimeProvider(my_ntp_provider);

    Serial.println("[Mode: UUID v7 (Time-Based)]");
    if (uuid.generate()) {
        char buf[37];
        uuid.toString(buf, sizeof(buf));
        Serial.print("Generated (Current Date): ");
        Serial.println(buf);
        // Output starts with approx: 018b7... (reflecting 2023)
        // Instead of: 0000... (reflecting 1970)
    }

    // --- SCENARIO B: NO CLOCK AVAILABLE (UUIDv4) ---
    
    Serial.println("\n[Mode: UUID v4 (Random)]");
    Serial.println("Warning: RTC failed or not available. Switching to v4.");

    // If we don't trust the clock, v7 is dangerous (collisions possible on reboot).
    // Switch to Version 4 (Pure Random).
    uuid.setVersion(UUID_VERSION_4);

    if (uuid.generate()) {
        char buf[37];
        uuid.toString(buf, sizeof(buf));
        Serial.print("Generated (Random):       ");
        Serial.println(buf);
        // Output looks like: xxxxxxxx-xxxx-4xxx-xxxx-xxxxxxxxxxxx
        // Notice the '4' in the third group.
    }
}

void loop() {
    // Nothing here
}
