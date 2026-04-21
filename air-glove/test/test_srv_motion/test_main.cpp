/* Placeholder Unity test. Real tests land in plan 06. */

#include <unity.h>

void setUp(void) {}
void tearDown(void) {}

static void test_placeholder(void) {
    TEST_ASSERT_TRUE(true);
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_placeholder);
    return UNITY_END();
}
