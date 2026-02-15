/*
 * EXAMPLE 4: High Performance Benchmark & Overflow Policy
 * 
 * Demonstrates:
 * 1. Generating UUIDs in a tight loop.
 * 2. Throughput measurement (UUIDs per second).
 * 3. Configuring OVERFLOW POLICY (Fail Fast vs Wait).
 */

#include <Arduino.h>
#include "UUID7.h"

UUID7 uuid;

void run_benchmark(const char* name) {
    Serial.print("Benchmarking: "); Serial.println(name);
    
    uint32_t start = micros();
    uint32_t count = 0;
    const uint32_t DURATION_US = 1000000; // 1 second test

    while (micros() - start < DURATION_US) {
        if (uuid.generate()) {
            count++;
        } else {
            // Depending on policy, this might happen on extremely fast CPUs
            // if the random counter overflows within 1ms.
        }
    }

    float rate = (float)count;
    Serial.print("Result: "); Serial.print(count); Serial.println(" UUIDs/sec");
    Serial.print("Time per UUID: "); Serial.print(1000000.0 / rate); Serial.println(" us");
    Serial.println("-----------------------------");
}

void setup() {
    Serial.begin(115200);
    while(!Serial);
    Serial.println("\n--- UUIDv7 Performance Benchmark ---");

    // TEST 1: Fail Fast (Default)
    // If the 74-bit counter overflows in the same millisecond, generate() returns false.
    uuid.setOverflowPolicy(UUID_OVERFLOW_FAIL_FAST);
    run_benchmark("Policy: FAIL_FAST");

    // TEST 2: Wait (Spin-lock)
    // If overflow occurs, the library waits for the next millisecond tick.
    // This guarantees a UUID is returned (unless clock fails), but varies execution time.
    uuid.setOverflowPolicy(UUID_OVERFLOW_WAIT);
    run_benchmark("Policy: WAIT (Busy Loop)");

    // Production Tip:
    // On AVR (16MHz), 'FAIL_FAST' is rarely triggered because generating one UUID
    // takes ~50-100us, so you can't physically generate 2^74 UUIDs in 1ms.
    // On ESP32 (240MHz), it's faster, but still unlikely to overflow 74 bits.
    // However, configuring WAIT is safer for data-critical logs.
}

void loop() {}
