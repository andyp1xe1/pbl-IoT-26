/* test_srv_motion — native Unity tests for the mix-matrix cursor mapping.
 *
 * Run with:  pio test -e native -f test_srv_motion
 *
 * Verifies the linear-mix pipeline: signal vector in, dx/dy out, with
 * sensitivity, radial deadzone, gain curve, and clutch. No hardware needed.
 */

#include <unity.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#include "srv_motion.h"

static motion_config_t default_cfg(void)
{
    motion_config_t c = {};
    /* Default: pitch+yaw → dx (with yaw negated), roll → dy.
     * Matches firmware defaults so tests reflect production behaviour. */
    c.mix_x_milli[AG_MIX_PITCH] = +1000;
    c.mix_x_milli[AG_MIX_YAW]   = -1000;
    c.mix_y_milli[AG_MIX_ROLL]  = -1700;
    c.sens_x_milli = 1000;
    c.sens_y_milli = 1000;
    c.deadzone_rad = 0.004f;
    return c;
}

static void zero_signals(float s[AG_MIX_COUNT])
{
    memset(s, 0, sizeof(float) * AG_MIX_COUNT);
}

void setUp(void)    { motion_config_t c = default_cfg(); srv_motion_init(&c); }
void tearDown(void) {}

void test_null_args_rejected(void)
{
    int8_t dx, dy;
    TEST_ASSERT_EQUAL(AG_ERR_ARG, srv_motion_update(NULL, &dx, &dy));
    float s[AG_MIX_COUNT] = {0};
    TEST_ASSERT_EQUAL(AG_ERR_ARG, srv_motion_update(s, NULL, &dy));
    TEST_ASSERT_EQUAL(AG_ERR_ARG, srv_motion_update(s, &dx, NULL));
    TEST_ASSERT_EQUAL(AG_ERR_ARG, srv_motion_init(NULL));
}

void test_init_rejects_zero_sens(void)
{
    motion_config_t c = default_cfg();
    c.sens_x_milli = 0;
    TEST_ASSERT_EQUAL(AG_ERR_ARG, srv_motion_init(&c));
}

void test_zero_signals_yield_zero_output(void)
{
    float s[AG_MIX_COUNT] = {0};
    int8_t dx, dy;
    for (int i = 0; i < 20; ++i) srv_motion_update(s, &dx, &dy);
    TEST_ASSERT_EQUAL_INT8(0, dx);
    TEST_ASSERT_EQUAL_INT8(0, dy);
}

void test_pitch_drives_dx_positive(void)
{
    float s[AG_MIX_COUNT];
    zero_signals(s);
    s[AG_MIX_PITCH] = 0.05f;
    int8_t dx = 0, dy = 0;
    for (int i = 0; i < 30; ++i) srv_motion_update(s, &dx, &dy);
    TEST_ASSERT_GREATER_THAN_INT8(0, dx);
    TEST_ASSERT_EQUAL_INT8(0, dy);
}

void test_yaw_drives_dx_negative(void)
{
    float s[AG_MIX_COUNT];
    zero_signals(s);
    s[AG_MIX_YAW] = 0.05f;
    int8_t dx = 0, dy = 0;
    for (int i = 0; i < 30; ++i) srv_motion_update(s, &dx, &dy);
    TEST_ASSERT_LESS_THAN_INT8(0, dx);
    TEST_ASSERT_EQUAL_INT8(0, dy);
}

void test_roll_drives_dy_negative(void)
{
    float s[AG_MIX_COUNT];
    zero_signals(s);
    s[AG_MIX_ROLL] = 0.05f;
    int8_t dx = 0, dy = 0;
    for (int i = 0; i < 30; ++i) srv_motion_update(s, &dx, &dy);
    TEST_ASSERT_EQUAL_INT8(0, dx);
    TEST_ASSERT_LESS_THAN_INT8(0, dy);
}

void test_sensitivity_scales_output(void)
{
    motion_config_t c = default_cfg();
    c.sens_x_milli = 2000;
    srv_motion_init(&c);
    float s[AG_MIX_COUNT];
    zero_signals(s);
    s[AG_MIX_PITCH] = 0.02f;
    int8_t dx_2x = 0, dy = 0;
    for (int i = 0; i < 30; ++i) srv_motion_update(s, &dx_2x, &dy);

    c.sens_x_milli = 1000;
    srv_motion_init(&c);
    int8_t dx_1x = 0;
    for (int i = 0; i < 30; ++i) srv_motion_update(s, &dx_1x, &dy);

    TEST_ASSERT_GREATER_THAN_INT8(dx_1x, dx_2x);
}

void test_clutch_zeros_output(void)
{
    float s[AG_MIX_COUNT];
    zero_signals(s);
    s[AG_MIX_PITCH] = 0.1f;
    int8_t dx, dy;
    for (int i = 0; i < 20; ++i) srv_motion_update(s, &dx, &dy);
    TEST_ASSERT_NOT_EQUAL_INT8(0, dx);

    srv_motion_set_clutch(true);
    srv_motion_update(s, &dx, &dy);
    TEST_ASSERT_EQUAL_INT8(0, dx);
    TEST_ASSERT_EQUAL_INT8(0, dy);
}

void test_deadzone_kills_tiny_motion(void)
{
    float s[AG_MIX_COUNT];
    zero_signals(s);
    s[AG_MIX_PITCH] = 0.001f;
    int8_t dx, dy;
    for (int i = 0; i < 50; ++i) srv_motion_update(s, &dx, &dy);
    TEST_ASSERT_EQUAL_INT8(0, dx);
    TEST_ASSERT_EQUAL_INT8(0, dy);
}

void test_disabled_axis_contributes_nothing(void)
{
    motion_config_t c = default_cfg();
    srv_motion_init(&c);
    float s[AG_MIX_COUNT];
    zero_signals(s);
    s[AG_MIX_AX] = 5.0f;
    int8_t dx, dy;
    for (int i = 0; i < 30; ++i) srv_motion_update(s, &dx, &dy);
    TEST_ASSERT_EQUAL_INT8(0, dx);
    TEST_ASSERT_EQUAL_INT8(0, dy);
}

int main(int /*argc*/, char ** /*argv*/)
{
    UNITY_BEGIN();
    RUN_TEST(test_null_args_rejected);
    RUN_TEST(test_init_rejects_zero_sens);
    RUN_TEST(test_zero_signals_yield_zero_output);
    RUN_TEST(test_pitch_drives_dx_positive);
    RUN_TEST(test_yaw_drives_dx_negative);
    RUN_TEST(test_roll_drives_dy_negative);
    RUN_TEST(test_sensitivity_scales_output);
    RUN_TEST(test_clutch_zeros_output);
    RUN_TEST(test_deadzone_kills_tiny_motion);
    RUN_TEST(test_disabled_axis_contributes_nothing);
    return UNITY_END();
}
