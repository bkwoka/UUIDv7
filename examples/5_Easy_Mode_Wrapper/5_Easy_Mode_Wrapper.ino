/*
 * EXAMPLE 5: Easy Mode Wrapper
 * 
 * Demonstrates the use of "EasyUUID7" class which provides:
 * - Integration with Arduino String class.
 * - toCharArray() method similar to other libraries.
 * - Simplified generation (auto-retry).
 * 
 * TRADE-OFF:
 * EasyUUID7 consumes 37 bytes MORE RAM per instance than the standard UUID7 class.
 * Use standard UUID7 for memory-constrained projects (AVR).
 */

#include <Arduino.h>
#include "EasyUUID7.h" // Include the wrapper

// Use EasyUUID7 instead of UUID7
EasyUUID7 uuid;

void setup() {
    Serial.begin(115200);
    while(!Serial);
    Serial.println("\n--- UUIDv7 Easy Mode ---");

    // 1. Simple Generation (No boolean check needed, it retries automatically)
    uuid.generate();

    // 2. Access as char* (C-style string)
    // The pointer is valid as long as the object exists.
    Serial.print("As char*: ");
    Serial.println(uuid.toCharArray());

    // 3. Access as Arduino String
    String myID = uuid.toString();
    Serial.print("As String: ");
    Serial.println(myID);

    // 4. String Concatenation
    String url = "https://api.example.com/v1/devices/" + String(uuid);
    Serial.print("URL: ");
    Serial.println(url);

    // 5. Direct Print (Inherited Printable interface)
    Serial.print("Direct: ");
    Serial.println(uuid);
}

void loop() {
    // Generate new ID every second
    uuid.generate();
    Serial.println(uuid);
    delay(1000);
}
