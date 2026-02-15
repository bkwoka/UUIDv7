/*
 * EXAMPLE 2: Robust Persistence (Double-Slot + CRC32)
 * 
 * CRITICAL FOR PRODUCTION:
 * UUIDv7 relies on monotonically increasing time. If a device reboots,
 * the system clock resets. To prevent generating duplicate UUIDs (time regression),
 * we must save the last timestamp and restore it with a "Safety Jump".
 * 
 * Features:
 * - Ping-Pong Storage: Writes to Slot A then Slot B. Protects against power loss during write.
 * - CRC32: Detects corrupted EEPROM data.
 * - Lazy Write: Only writes to EEPROM once every X seconds to save flash cycles (Wear Leveling).
 */

#include <Arduino.h>
#include <EEPROM.h>
#include "UUID7.h"

// --- CONFIGURATION ---
struct StorageSlot {
    uint64_t timestamp;
    uint32_t crc;
};

// EEPROM Layout
const int SLOT_A_ADDR = 0;
const int SLOT_B_ADDR = sizeof(StorageSlot); // 12 bytes
// Write interval. 10s = ~3 million writes endurance / 10s = ~1 year of continuous operation
// if writing constantly. In reality, much longer.
const uint32_t SAVE_INTERVAL_MS = 10000; 

UUID7 uuid;

// --- HELPERS ---
uint32_t calcCRC32(const uint8_t *data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 1) crc = (crc >> 1) ^ 0xEDB88320;
            else         crc >>= 1;
        }
    }
    return ~crc;
}

// --- ADAPTER IMPLEMENTATION ---

uint64_t load_from_eeprom(void* ctx) {
    StorageSlot sA, sB;
    EEPROM.get(SLOT_A_ADDR, sA);
    EEPROM.get(SLOT_B_ADDR, sB);

    bool okA = (calcCRC32((uint8_t*)&sA.timestamp, 8) == sA.crc);
    bool okB = (calcCRC32((uint8_t*)&sB.timestamp, 8) == sB.crc);

    if (okA && okB) return (sA.timestamp > sB.timestamp) ? sA.timestamp : sB.timestamp;
    if (okA) return sA.timestamp;
    if (okB) return sB.timestamp;
    return 0; // Fresh start
}

void save_to_eeprom(uint64_t ts, void* ctx) {
    StorageSlot sA, sB;
    EEPROM.get(SLOT_A_ADDR, sA);
    EEPROM.get(SLOT_B_ADDR, sB);

    bool okA = (calcCRC32((uint8_t*)&sA.timestamp, 8) == sA.crc);
    bool okB = (calcCRC32((uint8_t*)&sB.timestamp, 8) == sB.crc);

    // Overwrite strategy: Overwrite corrupt slot, or the older one
    int target = SLOT_A_ADDR;
    if (!okA) target = SLOT_A_ADDR;
    else if (!okB) target = SLOT_B_ADDR;
    else target = (sA.timestamp < sB.timestamp) ? SLOT_A_ADDR : SLOT_B_ADDR;

    StorageSlot newS;
    newS.timestamp = ts;
    newS.crc = calcCRC32((uint8_t*)&newS.timestamp, 8);
    
    EEPROM.put(target, newS);
    #if defined(ESP8266) || defined(ESP32)
        EEPROM.commit();
    #endif
    
    Serial.print("[Persistence] State saved to EEPROM @ ");
    Serial.println(target);
}

void setup() {
    Serial.begin(115200);
    #if defined(ESP8266) || defined(ESP32)
        EEPROM.begin(512);
    #endif

    // Dependency Injection: Plug in the storage mechanism
    uuid.setStorage(load_from_eeprom, save_to_eeprom, nullptr, SAVE_INTERVAL_MS);
    
    // CRITICAL: Load state before generating!
    // This performs the "Safety Jump" (sets internal clock ahead of last saved time)
    uuid.load(); 
    
    Serial.println("UUIDv7 Initialized with Persistence.");
}

void loop() {
    if(uuid.generate()) {
        char buf[37];
        uuid.toString(buf, 37);
        Serial.println(buf);
    }
    delay(2000); 
}
