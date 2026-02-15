#ifdef STANDALONE_TEST
    #include <assert.h>
    #include <stdio.h>
    #include <string.h>
    #define TEST_ASSERT_TRUE(cond) assert(cond)
    #define TEST_ASSERT_FALSE(cond) assert(!(cond))
    #define TEST_ASSERT_EQUAL_UINT8(expected, actual) assert((expected) == (actual))
    #define TEST_ASSERT_EQUAL_MEMORY(expected, actual, len) assert(memcmp(expected, actual, len) == 0)
    #define TEST_ASSERT_EQUAL_STRING(expected, actual) assert(strcmp(expected, actual) == 0)
    #define RUN_TEST(func) do { printf("Running %s...", #func); func(); printf(" OK\n"); } while(0)
    #define UNITY_BEGIN() printf("Starting Standalone Tests...\n")
    #define UNITY_END() printf("All tests passed!\n")
#else
    #include <unity.h>
    #include <string.h>
#endif

#include "UUID7.h"

// --- TEST CASES ---

void test_version_and_variant() {
    UUID7 g;
    bool ok = g.generate();
    TEST_ASSERT_TRUE(ok);
    const uint8_t* b = g.data();
    uint8_t ver = (b[6] >> 4) & 0x0F;
    TEST_ASSERT_EQUAL_UINT8(7, ver);
    uint8_t var = (b[8] >> 6);
    TEST_ASSERT_EQUAL_UINT8(2, var);
}

void test_version_v4() {
    UUID7 g;
    g.setVersion(UUID_VERSION_4);
    bool ok = g.generate();
    TEST_ASSERT_TRUE(ok);
    const uint8_t* b = g.data();
    uint8_t ver = (b[6] >> 4) & 0x0F;
    TEST_ASSERT_EQUAL_UINT8(4, ver);
    uint8_t var = (b[8] >> 6);
    TEST_ASSERT_EQUAL_UINT8(2, var);
}

void test_toString_and_parse() {
    UUID7 g;
    bool ok = g.generate();
    TEST_ASSERT_TRUE(ok);
    char out[37];
    TEST_ASSERT_TRUE(g.toString(out, sizeof(out)));
    uint8_t parsed[16];
    TEST_ASSERT_TRUE(UUID7::parseFromString(out, parsed));
    TEST_ASSERT_EQUAL_MEMORY(g.data(), parsed, 16);
}

// Mock time
static uint64_t mock_time_val = 1000;
static uint64_t mock_now_ms(void*) { return mock_time_val; }

// Mock persistence
static uint64_t mock_nvs_storage = 0;
static uint64_t last_saved_value_check = 0;
static int save_call_count = 0;

static void mock_save_fn(uint64_t ts, void*) {
    mock_nvs_storage = ts;
    last_saved_value_check = ts;
    save_call_count++;
}

static uint64_t mock_load_fn(void*) {
    return mock_nvs_storage;
}

void test_persistence() {
    mock_nvs_storage = 5000; 
    mock_time_val = 100;     
    save_call_count = 0;
    
    UUID7 g(nullptr, nullptr, mock_now_ms, nullptr);
    g.setStorage(mock_load_fn, mock_save_fn, nullptr, 1000);
    g.load();
    
    // Safety Jump check
    g.generate();
    const uint8_t* val = g.data();
    uint64_t generated_ts = 0;
    for(int i=0; i<6; ++i) generated_ts = (generated_ts << 8) | val[i];
    
    // Expect 6000 (5000 + 1000 interval)
    if (generated_ts != 6000) {
        #ifdef STANDALONE_TEST
        printf("Expected 6000, got %llu\n", (long long unsigned)generated_ts);
        #endif
    }
    TEST_ASSERT_TRUE(generated_ts == 6000); 
    TEST_ASSERT_EQUAL_UINT8(0, save_call_count);
    
    // Lazy Write check
    mock_time_val = 6001;
    g.generate();
    TEST_ASSERT_EQUAL_UINT8(1, save_call_count);
    TEST_ASSERT_TRUE(mock_nvs_storage == 6001);
}

// Deterministic vectors
static uint8_t mock_rng_val = 0;
static void deterministic_rng(uint8_t* dest, size_t len, void*) {
    for(size_t i=0; i<len; i++) dest[i] = mock_rng_val++;
}

void test_deterministic_vectors() {
    mock_rng_val = 0;
    mock_time_val = 0x01856E83F300ULL; 
    
    UUID7 g(deterministic_rng, nullptr, mock_now_ms, nullptr);
    TEST_ASSERT_TRUE(g.generate());
    
    char output[37];
    g.toString(output, sizeof(output));
    
    const char* expected = "01856e83-f300-7607-8809-0a0b0c0d0e0f";
    if (strcmp(output, expected) != 0) {
        #ifdef STANDALONE_TEST
        printf("Mismatch!\nE: %s\nA: %s\n", expected, output);
        #endif
    }
    TEST_ASSERT_EQUAL_STRING(expected, output);
}

void test_monotonicity() {
    UUID7 g(nullptr, nullptr, mock_now_ms, nullptr);
    g.generate();
    uint8_t uuid1[16];
    memcpy(uuid1, g.data(), 16);

    g.generate();
    uint8_t uuid2[16];
    memcpy(uuid2, g.data(), 16);

    int cmp = memcmp(uuid2, uuid1, 16);
    TEST_ASSERT_TRUE(cmp > 0);
    TEST_ASSERT_EQUAL_MEMORY(uuid1, uuid2, 6);
}

static void failing_rng(uint8_t* dest, size_t len, void*) {
    memset(dest, 0, len);
}

void test_rng_health_check() {
    UUID7 g7(failing_rng, nullptr, mock_now_ms, nullptr);
    TEST_ASSERT_TRUE(g7.generate() == false);
    
    UUID7 g4(failing_rng, nullptr, mock_now_ms, nullptr);
    g4.setVersion(UUID_VERSION_4);
    TEST_ASSERT_TRUE(g4.generate() == false);
}

static int dynamic_time_calls = 0;
static uint64_t mock_now_ms_overflow(void*) {
    dynamic_time_calls++;
    if (dynamic_time_calls > 10) {
        mock_time_val++;
        dynamic_time_calls = 0;
    }
    return mock_time_val;
}

static void overflow_rng(uint8_t* dest, size_t len, void*) {
    memset(dest, 0xFF, len);
}

void test_overflow_policy() {
    UUID7 g;
    TEST_ASSERT_TRUE(g.getOverflowPolicy() == UUID_OVERFLOW_FAIL_FAST);
    g.setOverflowPolicy(UUID_OVERFLOW_WAIT);
    TEST_ASSERT_TRUE(g.getOverflowPolicy() == UUID_OVERFLOW_WAIT);
}

void test_overflow_policy_wait() {
    mock_time_val = 5000;
    dynamic_time_calls = 0;
    
    UUID7 g(overflow_rng, nullptr, mock_now_ms_overflow, nullptr);
    g.setOverflowPolicy(UUID_OVERFLOW_WAIT);
    
    // First call ok
    TEST_ASSERT_TRUE(g.generate()); 
    // Second call triggers overflow -> wait loop -> time increment -> success
    TEST_ASSERT_TRUE(g.generate());
    
    const uint8_t* b = g.data();
    uint64_t final_ts = 0;
    for(int i=0; i<6; i++) final_ts = (final_ts << 8) | b[i];
    TEST_ASSERT_TRUE(final_ts > 5000);
}

// --- NEW TESTS (100% Coverage) ---

void test_api_edge_cases() {
    UUID7 g;
    g.generate();

    // 1. toString buffer sizes
    char buf37[37];
    char buf36[36];
    TEST_ASSERT_TRUE(g.toString(buf37, 37)); // OK
    TEST_ASSERT_TRUE(g.toString(buf36, 36) == false); // Too small
    TEST_ASSERT_TRUE(g.toString(nullptr, 37) == false); // Null ptr

    // 2. parseFromString edge cases
    uint8_t out[16];
    TEST_ASSERT_TRUE(UUID7::parseFromString(nullptr, out) == false);
    TEST_ASSERT_TRUE(UUID7::parseFromString(buf37, nullptr) == false);
    TEST_ASSERT_TRUE(UUID7::parseFromString("too-short", out) == false);
    
    // Invalid format (wrong dashes)
    TEST_ASSERT_TRUE(UUID7::parseFromString("01856e83af300a7607a88090a0b0c0d0e0f", out) == false);
    // Invalid characters
    TEST_ASSERT_TRUE(UUID7::parseFromString("01856e83-f300-7607-8809-0a0b0c0d0e0g", out) == false);

    // 3. Constructor with nullptrs
    UUID7 g_null(nullptr, nullptr, nullptr, nullptr);
    TEST_ASSERT_TRUE(g_null.generate()); // Should use defaults
}

void test_time_scenarios() {
    // 1. Clock Regression
    mock_time_val = 10000;
    UUID7 g(nullptr, nullptr, mock_now_ms, nullptr);
    g.generate();
    uint64_t ts1 = 0;
    for(int i=0; i<6; i++) ts1 = (ts1 << 8) | g.data()[i];
    TEST_ASSERT_TRUE(ts1 == 10000);

    mock_time_val = 5000; // Regression!
    g.generate();
    uint64_t ts2 = 0;
    for(int i=0; i<6; i++) ts2 = (ts2 << 8) | g.data()[i];
    TEST_ASSERT_TRUE(ts2 == 10000); // Should stay at last_ts

    // 2. Time Zero
    mock_time_val = 0;
    UUID7 g_zero(nullptr, nullptr, mock_now_ms, nullptr);
    TEST_ASSERT_TRUE(g_zero.generate() == false);

    // 3. Max Timestamp (48-bit)
    mock_time_val = 0x0000FFFFFFFFFFFFULL;
    UUID7 g_max(nullptr, nullptr, mock_now_ms, nullptr);
    TEST_ASSERT_TRUE(g_max.generate());
    uint64_t ts_max = 0;
    for(int i=0; i<6; i++) ts_max = (ts_max << 8) | g_max.data()[i];
    TEST_ASSERT_EQUAL_MEMORY(g_max.data(), "\xFF\xFF\xFF\xFF\xFF\xFF", 6);
}

void test_persistence_scenarios() {
    // 1. Load from empty storage
    mock_nvs_storage = 0;
    UUID7 g(nullptr, nullptr, mock_now_ms, nullptr);
    g.setStorage(mock_load_fn, mock_save_fn, nullptr, 1000);
    g.load();
    mock_time_val = 1000;
    g.generate();
    uint64_t ts = 0;
    for(int i=0; i<6; i++) ts = (ts << 8) | g.data()[i];
    TEST_ASSERT_TRUE(ts == 1000); // No jump for 0

    // 2. Save with interval 0 (save on every generation)
    g.setStorage(mock_load_fn, mock_save_fn, nullptr, 0);
    save_call_count = 0;
    mock_time_val = 2000;
    g.generate();
    TEST_ASSERT_TRUE(save_call_count == 1);
    mock_time_val = 2001;
    g.generate();
    TEST_ASSERT_TRUE(save_call_count == 2);
}

void test_operators() {
    UUID7 g1(nullptr, nullptr, mock_now_ms, nullptr), 
          g2(nullptr, nullptr, mock_now_ms, nullptr);
    
    mock_time_val = 1000;
    g1.generate();
    mock_time_val = 2000;
    g2.generate();

    TEST_ASSERT_TRUE(g1 == g1);
    TEST_ASSERT_TRUE(g1 != g2);
    TEST_ASSERT_TRUE(g1 < g2);
}

void test_full_overflow_cycle() {
    // Sequence increment check (manual inspection of _nextRandom)
    // We can simulate this by causing collisions and checking the bits
    
    mock_time_val = 1000;
    // Use a predictable RNG for the base
    UUID7 g(deterministic_rng, nullptr, mock_now_ms, nullptr);
    mock_rng_val = 0;
    
    g.generate(); 
    // Initial random for timestamp 1000
    // Byte 6: 0x06 -> 0x76 (ver 7)
    // Byte 7: 0x07
    // Byte 8: 0x08 -> 0x88 (var 2)
    // Bytes 9-15: 0x09 0x0A 0x0B 0x0C 0x0D 0x0E 0x0F
    
    // Collide
    g.generate(); 
    // _nextRandom increments Bytes 15 down to 9
    // Byte 15 was 0x0F -> now 0x10
    TEST_ASSERT_TRUE(g.data()[15] == 0x10);
    
    // Force many increments to see carry to Byte 8 and above
    // (This is hard to do 2^56 times, but we can trust the logic 
    // if small carries work and we audit the bit masks)
    
    // Let's audit Byte 8 masking (variant 0b10xxxxxx)
    uint8_t b8 = g.data()[8];
    TEST_ASSERT_TRUE((b8 & 0xC0) == 0x80);
}

// --- NEW TESTS FOR MISSING COVERAGE ---

void test_full_overflow_cycle_detailed() {
    static uint8_t mock_rng_val[16];
    memset(mock_rng_val, 0, 16);
    mock_rng_val[15] = 0xFF; // Max out last byte to force carry to byte 14
    mock_rng_val[14] = 0xFE; 

    auto custom_rng = [](uint8_t* dest, size_t len, void*) {
        memcpy(dest, mock_rng_val, len);
    };

    mock_time_val = 1000; 
    UUID7 g(custom_rng, nullptr, mock_now_ms, nullptr);
    
    // First generate
    g.generate();
    const uint8_t* b = g.data();
    TEST_ASSERT_EQUAL_UINT8(0xFF, b[15]);
    TEST_ASSERT_EQUAL_UINT8(0xFE, b[14]);

    // Second generate (collision) -> triggers _nextRandom()
    // 0xFEFF + 1 = 0xFF00
    g.generate();
    TEST_ASSERT_EQUAL_UINT8(0x00, b[15]);
    TEST_ASSERT_EQUAL_UINT8(0xFF, b[14]);
    
    // Third generate (collision) -> triggers _nextRandom()
    // 0xFF00 + 1 = 0xFF01
    g.generate();
    TEST_ASSERT_EQUAL_UINT8(0x01, b[15]);
    TEST_ASSERT_EQUAL_UINT8(0xFF, b[14]);
}

void test_timestamp_wraparound_fallback() {
    mock_time_val = 10000;
    UUID7 g(nullptr, nullptr, mock_now_ms, nullptr);
    g.generate();
    TEST_ASSERT_EQUAL_UINT8(7, (g.data()[6] >> 4) & 0x0F);

    // Major regression: 10000 -> 0 (diff 10000 > 10000)
    // Wait, it needs to be GREATER than 10000. So 10000 -> 10001? No regression.
    // 10000 -> 0. diff is 10000. My code says "now_ms + 10000 < last_ts".
    // 0 + 10000 < 10000 is FALSE.
    // So 0 + 10000 < 10001 is TRUE.
    
    mock_time_val = 20000;
    g.generate(); // last_ts becomes 20000
    
    mock_time_val = 5000; // 5000 + 10000 < 20000 is TRUE
    g.generate();
    TEST_ASSERT_EQUAL_UINT8(4, (g.data()[6] >> 4) & 0x0F);
}

void test_parse_uppercase_hex() {
    UUID7 g;
    g.generate();
    char buf[37];
    g.toString(buf, sizeof(buf));

    for (char* p = buf; *p; p++) {
        if (*p >= 'a' && *p <= 'f') *p = *p - 'a' + 'A';
    }

    uint8_t parsed[16];
    TEST_ASSERT_TRUE(UUID7::parseFromString(buf, parsed));
    TEST_ASSERT_EQUAL_MEMORY(g.data(), parsed, 16);
}

void test_max_timestamp_masking() {
    mock_time_val = 0xFFFFFFFFFFFFFFFFULL; 
    UUID7 g(nullptr, nullptr, mock_now_ms, nullptr);
    TEST_ASSERT_TRUE(g.generate());
    const uint8_t* b = g.data();
    for(int i=0; i<6; i++) TEST_ASSERT_EQUAL_UINT8(0xFF, b[i]);
    TEST_ASSERT_EQUAL_UINT8(7, (b[6] >> 4) & 0x0F);
    TEST_ASSERT_EQUAL_UINT8(2, (b[8] >> 6) & 0x03);
}

void test_nextRandom_mask_preservation() {
    // We want to verify that _nextRandom (called via generate in same ms)
    // doesn't stomp on version/variant bits even if a large carry happens.
    
    // Mock RNG to return almost-overflow 74-bit random field
    static uint8_t almost_full[16];
    memset(almost_full, 0xFF, 16);
    // bytes 0-5 are timestamp (will be overwritten)
    almost_full[6] = 0x0F; // random_a low (high 4 are version)
    almost_full[7] = 0xFF; // random_a high
    almost_full[8] = 0x3F; // random_b high (high 2 are variant)
    // bytes 9-15 are 0xFF (random_b)

    auto special_rng = [](uint8_t* dest, size_t len, void*) {
        memcpy(dest, almost_full, 16);
    };

    mock_time_val = 1000;
    UUID7 g(special_rng, nullptr, mock_now_ms, nullptr);
    g.generate();
    
    // Verify initial version/variant
    TEST_ASSERT_EQUAL_UINT8(7, (g.data()[6] >> 4) & 0x0F);
    TEST_ASSERT_EQUAL_UINT8(2, (g.data()[8] >> 6) & 0x03);

    // Call again in same ms to trigger _nextRandom() overflow
    g.generate();
    
    // Verify masked bits still correct
    TEST_ASSERT_EQUAL_UINT8(7, (g.data()[6] >> 4) & 0x0F);
    TEST_ASSERT_EQUAL_UINT8(2, (g.data()[8] >> 6) & 0x03);
}
#if !defined(ARDUINO)
#include <sstream>
void test_ostream_operator() {
    UUID7 g;
    g.generate();
    
    char buf[37];
    g.toString(buf, 37);

    std::stringstream ss;
    ss << g;
    TEST_ASSERT_EQUAL_STRING(buf, ss.str().c_str());
}
#endif

void test_regression_threshold() {
    TEST_ASSERT_TRUE(UUID7_REGRESSION_THRESHOLD_MS > 0);
}

void test_from_bytes_and_formatting() {
    uint8_t raw[16] = {
        0x01, 0x8D, 0x96, 0x0E, 0x2B, 0x77, 0x7F, 0x8D,
        0x9C, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0
    };
    UUID7 g;
    g.fromBytes(raw);
    
    char buf[37];
    // Default
    g.toString(buf, 37);
    TEST_ASSERT_EQUAL_STRING("018d960e-2b77-7f8d-9c34-56789abcdef0", buf);
    
    // Uppercase
    g.toString(buf, 37, true, true);
    TEST_ASSERT_EQUAL_STRING("018D960E-2B77-7F8D-9C34-56789ABCDEF0", buf);
    
    // Raw lowercase
    g.toString(buf, 37, false, false);
    TEST_ASSERT_EQUAL_STRING("018d960e2b777f8d9c3456789abcdef0", buf);
    
    // Raw uppercase
    g.toString(buf, 37, true, false);
    TEST_ASSERT_EQUAL_STRING("018D960E2B777F8D9C3456789ABCDEF0", buf);
}

void test_parse_formats() {
    // Generate a known UUID
    UUID7 g;
    g.generate();
    
    // 1. Test Standard (Dashed) - 36 chars
    char bufDashed[37];
    g.toString(bufDashed, 37, false, true);
    
    uint8_t out1[16];
    TEST_ASSERT_TRUE(UUID7::parseFromString(bufDashed, out1));
    TEST_ASSERT_EQUAL_MEMORY(g.data(), out1, 16);

    // 2. Test Raw (No Dashes) - 32 chars
    char bufRaw[33]; // 32 + null
    g.toString(bufRaw, 33, false, false);
    
    uint8_t out2[16];
    TEST_ASSERT_TRUE(UUID7::parseFromString(bufRaw, out2));
    TEST_ASSERT_EQUAL_MEMORY(g.data(), out2, 16);

    // 3. Test Invalid Length
    TEST_ASSERT_FALSE(UUID7::parseFromString("too-short", out1));
    TEST_ASSERT_FALSE(UUID7::parseFromString("too-long-but-not-uuid-format-string", out1));
    
    // 4. Test Invalid Dash Position (using dashed length but wrong dashes)
    char broken[37];
    strcpy(broken, bufDashed);
    broken[8] = 'X'; // Overwrite first dash
    TEST_ASSERT_FALSE(UUID7::parseFromString(broken, out1));
}

// --- TEST RUNNER ---

void run_tests() {
    UNITY_BEGIN();
    RUN_TEST(test_version_and_variant);
    RUN_TEST(test_version_v4);
    RUN_TEST(test_toString_and_parse);
    RUN_TEST(test_monotonicity);
    RUN_TEST(test_persistence);
    RUN_TEST(test_deterministic_vectors);
    RUN_TEST(test_rng_health_check);
    RUN_TEST(test_overflow_policy);
    RUN_TEST(test_overflow_policy_wait);
    
    // Coverage Expansion
    RUN_TEST(test_api_edge_cases);
    RUN_TEST(test_time_scenarios);
    RUN_TEST(test_persistence_scenarios);
    RUN_TEST(test_operators);
    RUN_TEST(test_full_overflow_cycle);

    // New tests from task.txt
    RUN_TEST(test_full_overflow_cycle_detailed);
    RUN_TEST(test_timestamp_wraparound_fallback);
    RUN_TEST(test_parse_uppercase_hex);
    RUN_TEST(test_max_timestamp_masking);
    RUN_TEST(test_nextRandom_mask_preservation);
#if !defined(ARDUINO)
    RUN_TEST(test_ostream_operator);
#endif
    RUN_TEST(test_regression_threshold);
    RUN_TEST(test_from_bytes_and_formatting);
    RUN_TEST(test_parse_formats);
    
    UNITY_END();
}

// --- TEST EXECUTION ENTRY POINTS ---

#if defined(ARDUINO)
    void setup() {
        // Wait for serial if needed
        delay(2000);
        run_tests();
    }
    void loop() {}
#else
    int main(int argc, char **argv) {
        (void)argc; (void)argv;
        run_tests();
        return 0;
    }
#endif
