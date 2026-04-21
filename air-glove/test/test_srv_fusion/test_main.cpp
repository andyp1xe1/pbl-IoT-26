/* Placeholder Unity test so `pio test -e native` has something to run.
 * Real tests land in plan 05. */

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
