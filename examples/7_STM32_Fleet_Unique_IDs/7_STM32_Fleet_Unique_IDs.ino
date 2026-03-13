/*
 * EXAMPLE 7: STM32 Fleet-Unique IDs (mixEntropy + HAL UID)
 *
 * PROBLEM:
 * In a production fleet of STM32 devices, multiple boards may boot at the
 * same time without NTP sync. If they all start at millis() = 0, their
 * UUIDv7 timestamps will be identical — causing collisions.
 *
 * SOLUTION: mixEntropy()
 * STM32 microcontrollers have a 96-bit factory-programmed Unique Device ID
 * (accessible via HAL_GetUIDw0/1/2). Hashing this ID into the UUID's random
 * field guarantees spatial uniqueness across the entire fleet — even when
 * all devices share the same timestamp.
 *
 * This technique works WITHOUT a Real-Time Clock or NTP server.
 *
 * Compatible platforms: STM32F1 (BluePill), STM32F4 (Nucleo), and any
 * STM32 board using the Arduino STM32 HAL.
 */

#include <Arduino.h>
#include "UUID7.h"

UUID7 uuid;

/**
 * @brief Simple 64-bit hash mixing function (FNV-1a inspired).
 * Reduces the 96-bit STM32 UID to a 64-bit seed for mixEntropy().
 */
static uint64_t hash96to64(uint32_t w0, uint32_t w1, uint32_t w2) {
    uint64_t h = 0xcbf29ce484222325ULL; // FNV offset basis
    const uint32_t words[3] = {w0, w1, w2};
    for (int i = 0; i < 3; i++) {
        h ^= (uint64_t)words[i];
        h *= 0x00000100000001B3ULL; // FNV prime
    }
    return h;
}

void setup() {
    Serial.begin(115200);
    while (!Serial);
    Serial.println("\n--- UUIDv7 STM32 Fleet-Unique IDs ---");

    // 1. READ THE 96-BIT STM32 UNIQUE DEVICE ID
    //    Each chip has a different value burned in at the factory.
    uint32_t uid0 = HAL_GetUIDw0();
    uint32_t uid1 = HAL_GetUIDw1();
    uint32_t uid2 = HAL_GetUIDw2();

    Serial.print("Device UID: 0x");
    Serial.print(uid0, HEX); Serial.print("-");
    Serial.print(uid1, HEX); Serial.print("-");
    Serial.println(uid2, HEX);

    // 2. INJECT THE UID AS ENTROPY SEED
    //    This XORs into the random_b field of every generated UUID,
    //    ensuring no two devices in the fleet produce the same UUID.
    uint64_t entropy_seed = hash96to64(uid0, uid1, uid2);
    uuid.mixEntropy(entropy_seed);

    Serial.println("Entropy seed injected from hardware UID.");
    Serial.println("All UUIDs from this device are now globally unique.");
    Serial.println("---");

    // 3. GENERATE AND DISPLAY UUIDS
    //    Note: Without NTP/RTC, the timestamp starts at 1970-01-01.
    //    The UUIDs are still useful as unique opaque IDs — just not
    //    sortable by real-world time.
    for (int i = 0; i < 5; i++) {
        if (uuid.generate()) {
            char buf[37];
            uuid.toString(buf, sizeof(buf));
            Serial.println(buf);
        } else {
            Serial.println("ERROR: UUID generation failed.");
        }
    }

    // 4. COMBINING WITH A REAL-TIME CLOCK (optional upgrade)
    //
    //    If your board has an RTC or NTP access, inject a time provider
    //    for fully sortable, fleet-unique UUIDs:
    //
    //    uuid.setTimeProvider([](void*) -> uint64_t {
    //        return rtc.getUnixTime() * 1000ULL;
    //    });
    //
    //    With both mixEntropy() and setTimeProvider() active, you get:
    //    - Temporal sortability (from timestamp)
    //    - Spatial uniqueness (from UID)
    //    - Monotonicity (from sub-ms counter)
}

void loop() {
    // Generate one UUID per second
    if (uuid.generate()) {
        char buf[37];
        uuid.toString(buf, sizeof(buf));
        Serial.println(buf);
    }
    delay(1000);
}
