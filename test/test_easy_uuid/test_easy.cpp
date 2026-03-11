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

void run_easy_tests() {
    UNITY_BEGIN();
    RUN_TEST(test_easy_wrapper_generation);
    RUN_TEST(test_string_conversion);
    RUN_TEST(test_operators);
    RUN_TEST(test_easy_wrapper_retry_limit);
    RUN_TEST(test_easy_parse_cache);

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
