/* Unity tests for dd_mpu6050. Runs on target (env:esp32dev) only; this
 * suite is excluded from env:native via `test_ignore = test_dd_*` in
 * platformio.ini because it links against Wire / Arduino / esp_timer.
 *
 * Run with:
 *     pio test -e esp32dev -f test_dd_mpu6050
 */

#include <Arduino.h>
#include <Wire.h>
#include <esp_timer.h>
#include <math.h>
#include <unity.h>

#include "ag_pins.h"
#include "dd_mpu6050.h"

void setUp(void) {}
void tearDown(void) {}

/* Bypasses the driver — proves the board and I2C wiring are alive before
 * we blame the driver. */
static void test_who_am_i_is_0x68(void) {
    Wire.begin(AG_PIN_I2C_SDA, AG_PIN_I2C_SCL);
    Wire.setClock(400000);
    Wire.beginTransmission((uint8_t)0x68);
    Wire.write((uint8_t)0x75);
    TEST_ASSERT_EQUAL_UINT8(0, Wire.endTransmission(false));
    TEST_ASSERT_EQUAL_UINT8(1, Wire.requestFrom((uint8_t)0x68, (uint8_t)1));
    TEST_ASSERT_EQUAL_HEX8(0x68, (uint8_t)Wire.read());
}

static void test_init_returns_ok_on_connected_board(void) {
    TEST_ASSERT_EQUAL_INT(AG_OK, dd_mpu6050_init());
}

static void test_read_returns_ok_and_populates_sample(void) {
    imu_sample_t s = {};
    TEST_ASSERT_EQUAL_INT(AG_OK, dd_mpu6050_read(&s));
    TEST_ASSERT_NOT_EQUAL(0, s.t_us);
}

/* With the breakout flat on the bench, |a| must be one g ±5 %. */
static void test_gravity_magnitude_at_rest(void) {
    imu_sample_t s = {};
    float accum = 0.0f;
    const int N = 20;
    for (int i = 0; i < N; ++i) {
        TEST_ASSERT_EQUAL_INT(AG_OK, dd_mpu6050_read(&s));
        accum += sqrtf(s.ax * s.ax + s.ay * s.ay + s.az * s.az);
        delay(5);
    }
    const float mean_g = accum / (float)N;
    TEST_ASSERT_TRUE_MESSAGE(mean_g >= 9.5f && mean_g <= 10.1f,
                             "accel magnitude outside gravity window at rest");
}

/* Raw gyro bias (no correction in driver) is well under 3 °/s per axis
 * on a healthy MPU6050; 0.05 rad/s ~ 2.9 °/s. */
static void test_gyro_bias_at_rest(void) {
    imu_sample_t s = {};
    const int N = 20;
    for (int i = 0; i < N; ++i) {
        TEST_ASSERT_EQUAL_INT(AG_OK, dd_mpu6050_read(&s));
        TEST_ASSERT_TRUE_MESSAGE(
            fabsf(s.gx) < 0.05f && fabsf(s.gy) < 0.05f && fabsf(s.gz) < 0.05f,
            "gyro bias > 0.05 rad/s at rest");
        delay(5);
    }
}

/* 400 kHz + 14-byte burst + tiny overhead → ~350 us typical. 500 us
 * budget comes from architecture.md §5 timing table. */
static void test_read_latency_under_500us(void) {
    imu_sample_t s = {};
    TEST_ASSERT_EQUAL_INT(AG_OK, dd_mpu6050_read(&s));  /* warm-up */
    const int64_t t0 = esp_timer_get_time();
    TEST_ASSERT_EQUAL_INT(AG_OK, dd_mpu6050_read(&s));
    const int64_t dt = esp_timer_get_time() - t0;
    TEST_ASSERT_TRUE_MESSAGE(dt < 500, "dd_mpu6050_read exceeded 500 us budget");
}

void setup(void) {
    delay(2000); /* give the host serial a chance to settle */
    UNITY_BEGIN();
    RUN_TEST(test_who_am_i_is_0x68);
    RUN_TEST(test_init_returns_ok_on_connected_board);
    RUN_TEST(test_read_returns_ok_and_populates_sample);
    RUN_TEST(test_gravity_magnitude_at_rest);
    RUN_TEST(test_gyro_bias_at_rest);
    RUN_TEST(test_read_latency_under_500us);
    UNITY_END();
}

void loop(void) {}
