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

void run_easy_tests() {
    UNITY_BEGIN();
    RUN_TEST(test_easy_wrapper_generation);
    RUN_TEST(test_string_conversion);
    RUN_TEST(test_operators);
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
