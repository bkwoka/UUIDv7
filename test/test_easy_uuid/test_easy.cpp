#include <unity.h>
#include "EasyUUID7.h"

void test_easy_wrapper_generation() {
    EasyUUID7 easy;
    
    // First generation (auto-retry logic)
    easy.generate();
    
    // Check if buffer is filled
    char* str = easy.toCharArray();
    TEST_ASSERT_EQUAL_INT(36, strlen(str));
    TEST_ASSERT_EQUAL_CHAR('-', str[8]);
}

void test_string_conversion() {
    EasyUUID7 easy;
    easy.generate();
    
    String s = easy.toString();
    char* c = easy.toCharArray();
    
    // Check content equality
    TEST_ASSERT_TRUE(s.equals(c));
}

void test_operators() {
    EasyUUID7 easy;
    easy.generate();
    
    String s1 = easy; // Operator String()
    String s2 = easy.toString();
    
    TEST_ASSERT_TRUE(s1 == s2);
}

/**
 * @brief Mock RNG that always returns zero to simulate generation failure.
 * This triggers the retry logic in EasyUUID7.
 */
static void mock_failing_rng(uint8_t* dest, size_t len, void*) {
    memset(dest, 0, len); 
}

/**
 * @brief Verifies that the EasyUUID7 retry limit correctly prevents infinite loops
 * in case of persistent hardware or RNG failures.
 */
void test_easy_wrapper_retry_limit() {
    const uint16_t limit = 5;
    EasyUUID7 easy(limit);
    easy.setRandomSource(mock_failing_rng);
    
    // Generator should return false after exhaustive retries
    TEST_ASSERT_FALSE(easy.generate());
}


/**
 * @brief Verifies that EasyUUID7::parse() correctly synchronizes the internal 
 * string cache, preventing "stale state" issues.
 */
void test_easy_parse_cache() {
    EasyUUID7 easy;
    easy.generate(); // Fill cache initially
    
    UUID7 g;
    g.generate();
    char buf[37];
    g.toString(buf, sizeof(buf));
    
    // Parse new UUID string into the EasyUUID7 object
    TEST_ASSERT_TRUE(easy.parse(buf));
    
    // Internal cache should match the parsed string
    TEST_ASSERT_EQUAL_STRING(buf, easy.toCharArray());
}

/**
 * @brief Verifies that toCharArray() returns the Nil UUID string
 * ("00000000-0000-0000-0000-000000000000") when lazy generation fails
 * due to a persistent hardware RNG fault, instead of returning an
 * empty or undefined buffer.
 */
void test_toCharArray_nil_fallback() {
    // max_retries=1 so the retry loop exits immediately on RNG failure
    EasyUUID7 easy(1);
    easy.setRandomSource(mock_failing_rng);

    // Do NOT call generate() — rely on lazy init inside toCharArray()
    const char* result = easy.toCharArray();
    TEST_ASSERT_EQUAL_STRING("00000000-0000-0000-0000-000000000000", result);
}

/**
 * @brief Verifies that fromBytes() correctly updates the internal string
 * cache so that toCharArray() immediately reflects the loaded bytes
 * without requiring a separate generate() call.
 */
void test_fromBytes_cache_sync() {
    uint8_t raw[16] = {
        0x01, 0x8D, 0x96, 0x0E, 0x2B, 0x77, 0x7F, 0x8D,
        0x9C, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0
    };

    EasyUUID7 easy;
    easy.fromBytes(raw);

    // Compute expected string via base class for ground truth
    char expected[37];
    UUID7 ref;
    ref.fromBytes(raw);
    ref.toString(expected, sizeof(expected));

    // Cache must already reflect the loaded bytes — no generate() needed
    TEST_ASSERT_EQUAL_STRING(expected, easy.toCharArray());

    // String() operator must return the same value
    String s = easy.toString();
    TEST_ASSERT_TRUE(s.equals(expected));
}

void run_easy_tests() {
    UNITY_BEGIN();
    RUN_TEST(test_easy_wrapper_generation);
    RUN_TEST(test_string_conversion);
    RUN_TEST(test_operators);
    RUN_TEST(test_easy_wrapper_retry_limit);
    RUN_TEST(test_easy_parse_cache);

    RUN_TEST(test_toCharArray_nil_fallback);
    RUN_TEST(test_fromBytes_cache_sync);

    UNITY_END();
}

#if defined(ARDUINO)
    void setup() {
        delay(2000);
        run_easy_tests();
    }
    void loop() {}
#else
    int main(int argc, char **argv) {
        (void)argc; (void)argv;
        run_easy_tests();
        return 0;
    }
#endif
