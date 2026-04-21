/* Unity tests for dd_touch. Runs on target (env:esp32dev) only; excluded
 * from env:native via `test_ignore = test_dd_*` in platformio.ini because
 * it links against the ESP32 Arduino touchRead().
 *
 * Run with:
 *     pio test -e esp32dev -f test_dd_touch
 *
 * Pre-conditions on the bench:
 *   - ESP32 DevKit-C powered over USB.
 *   - Conductive pads attached to GPIO4/2/15/13 (thumb/index/middle/ring).
 *   - FINGERS OFF THE PADS until the prompt for the interactive test.
 */

#include <Arduino.h>
#include <esp_timer.h>
#include <math.h>
#include <unity.h>

#include "ag_pins.h"
#include "dd_touch.h"

void setUp(void) {}
void tearDown(void) {}

static void test_init_returns_ok(void) {
    TEST_ASSERT_EQUAL_INT(AG_OK, dd_touch_init());
}

/* Every pad should report a non-zero raw reading after init primes the
 * peripheral. A zero usually means a shorted/broken pad wire. */
static void test_read_populates_all_pads(void) {
    touch_sample_t s = {};
    TEST_ASSERT_EQUAL_INT(AG_OK, dd_touch_read(&s));
    for (int i = 0; i < TOUCH_PAD_COUNT; ++i) {
        TEST_ASSERT_TRUE_MESSAGE(s.raw[i] > 0, "pad raw reading is zero");
    }
    TEST_ASSERT_TRUE(s.t_us > 0);
}

static void test_timestamp_monotonic(void) {
    touch_sample_t a = {}, b = {};
    TEST_ASSERT_EQUAL_INT(AG_OK, dd_touch_read(&a));
    delay(2);
    TEST_ASSERT_EQUAL_INT(AG_OK, dd_touch_read(&b));
    TEST_ASSERT_TRUE_MESSAGE(b.t_us > a.t_us, "timestamps not strictly increasing");
}

/* E04 acceptance: baseline reading stable (< 5 % relative stddev) over 1 s. */
static void test_baseline_stable_1s(void) {
    touch_sample_t s = {};
    float sum  [TOUCH_PAD_COUNT] = {0};
    float sumsq[TOUCH_PAD_COUNT] = {0};
    const int N = 100;
    for (int k = 0; k < N; ++k) {
        TEST_ASSERT_EQUAL_INT(AG_OK, dd_touch_read(&s));
        for (int i = 0; i < TOUCH_PAD_COUNT; ++i) {
            const float v = (float)s.raw[i];
            sum  [i] += v;
            sumsq[i] += v * v;
        }
        delay(10);
    }
    for (int i = 0; i < TOUCH_PAD_COUNT; ++i) {
        const float mean = sum[i] / (float)N;
        const float var  = (sumsq[i] / (float)N) - (mean * mean);
        const float sd   = sqrtf(var > 0.0f ? var : 0.0f);
        const float rel  = (mean > 1.0f) ? (sd / mean) : 0.0f;
        TEST_ASSERT_TRUE_MESSAGE(rel < 0.05f, "pad idle relative stddev >= 5%");
    }
}

/* Running init twice (fingers off) should converge to similar baselines. */
static void test_calibration_repeatable(void) {
    touch_sample_t s = {};

    TEST_ASSERT_EQUAL_INT(AG_OK, dd_touch_init());
    TEST_ASSERT_EQUAL_INT(AG_OK, dd_touch_read(&s));
    uint16_t run1[TOUCH_PAD_COUNT];
    for (int i = 0; i < TOUCH_PAD_COUNT; ++i) run1[i] = s.raw[i];

    delay(500);

    TEST_ASSERT_EQUAL_INT(AG_OK, dd_touch_init());
    TEST_ASSERT_EQUAL_INT(AG_OK, dd_touch_read(&s));
    for (int i = 0; i < TOUCH_PAD_COUNT; ++i) {
        const float ratio = (float)s.raw[i] / (float)run1[i];
        TEST_ASSERT_TRUE_MESSAGE(ratio > 0.9f && ratio < 1.1f,
                                 "calibration re-run differs by > 10%");
    }
}

/* Interactive. Prompts the operator to touch the INDEX pad; passes if a
 * >= 30 % drop is observed AND `touched_mask` reports the pad within 3 s.
 * If no human is at the bench this test will fail — expected behaviour
 * for an interactive probe. */
static void test_touch_drops_reading_INDEX(void) {
    touch_sample_t s = {};
    TEST_ASSERT_EQUAL_INT(AG_OK, dd_touch_read(&s));
    const uint16_t idle = s.raw[TOUCH_PAD_INDEX];
    Serial.printf("\n>>> touch INDEX pad within 3 s (idle=%u)\n", idle);

    bool observed_drop = false;
    bool observed_mask = false;
    for (int k = 0; k < 60; ++k) {          /* 60 * 50 ms = 3 s window */
        TEST_ASSERT_EQUAL_INT(AG_OK, dd_touch_read(&s));
        if (s.raw[TOUCH_PAD_INDEX] < (uint16_t)((float)idle * 0.7f)) {
            observed_drop = true;
        }
        if (s.touched_mask & (1U << TOUCH_PAD_INDEX)) {
            observed_mask = true;
        }
        if (observed_drop && observed_mask) break;
        delay(50);
    }
    TEST_ASSERT_TRUE_MESSAGE(observed_drop, "no >= 30% drop observed on INDEX pad");
    TEST_ASSERT_TRUE_MESSAGE(observed_mask, "touched_mask never reported INDEX pad");
}

void setup(void) {
    delay(2000);
    Serial.begin(115200);

    UNITY_BEGIN();
    RUN_TEST(test_init_returns_ok);
    RUN_TEST(test_read_populates_all_pads);
    RUN_TEST(test_timestamp_monotonic);
    RUN_TEST(test_baseline_stable_1s);
    RUN_TEST(test_calibration_repeatable);
    RUN_TEST(test_touch_drops_reading_INDEX);
    UNITY_END();
}

void loop(void) {}
