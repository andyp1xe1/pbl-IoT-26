/* test_srv_motion — native Unity tests for the quaternion → cursor service.
 *
 * Run with:  pio test -e native -f test_srv_motion
 *
 * All inputs are synthesised quaternions — no hardware needed.
 */

#include <unity.h>
#include <math.h>
#include <stdint.h>

#include "srv_motion.h"

/* ── Helpers ──────────────────────────────────────────────────────────── */

static const quat_t IDENTITY = { 1.0f, 0.0f, 0.0f, 0.0f };

/* Unit quaternion representing rotation of `theta` radians around X (roll). */
static quat_t quat_x(float theta)
{
    const float h = theta * 0.5f;
    quat_t q = { cosf(h), sinf(h), 0.0f, 0.0f };
    return q;
}

/* Unit quaternion for rotation of `theta` radians around Y (pitch). */
static quat_t quat_y(float theta)
{
    const float h = theta * 0.5f;
    quat_t q = { cosf(h), 0.0f, sinf(h), 0.0f };
    return q;
}

/* Unit quaternion for rotation of `theta` radians around Z (yaw). */
static quat_t quat_z(float theta)
{
    const float h = theta * 0.5f;
    quat_t q = { cosf(h), 0.0f, 0.0f, sinf(h) };
    return q;
}

static motion_config_t cfg_default(void)
{
    motion_config_t c;
    c.deadzone_rad = 0.02f;
    c.gain_low     = 400.0f;
    c.gain_exp     = 1.6f;
    c.velocity_cap = 127.0f;
    c.gain_y_scale = 1.0f;
    return c;
}

static void prime_with_identity(void)
{
    int8_t dx, dy;
    srv_motion_update(&IDENTITY, 0.01f, &dx, &dy);   /* first call caches */
}

/* ── Fixture ──────────────────────────────────────────────────────────── */

void setUp(void)
{
    motion_config_t c = cfg_default();
    srv_motion_init(&c);
}

void tearDown(void) {}

/* ── Tests ────────────────────────────────────────────────────────────── */

/* 1. Init rejects NULL. */
static void test_init_rejects_null(void)
{
    TEST_ASSERT_EQUAL_INT(AG_ERR_ARG, srv_motion_init(nullptr));
}

/* 2. Init rejects out-of-range fields. */
static void test_init_rejects_bad_config(void)
{
    motion_config_t c;

    c = cfg_default(); c.deadzone_rad = -0.01f;
    TEST_ASSERT_EQUAL_INT(AG_ERR_ARG, srv_motion_init(&c));

    c = cfg_default(); c.gain_low = 0.0f;
    TEST_ASSERT_EQUAL_INT(AG_ERR_ARG, srv_motion_init(&c));

    c = cfg_default(); c.gain_exp = 0.5f;           /* must be >= 1.0 */
    TEST_ASSERT_EQUAL_INT(AG_ERR_ARG, srv_motion_init(&c));

    c = cfg_default(); c.velocity_cap = 0.0f;
    TEST_ASSERT_EQUAL_INT(AG_ERR_ARG, srv_motion_init(&c));
}

/* 3. First call after init always returns zero regardless of input. */
static void test_first_call_returns_zero(void)
{
    int8_t dx = 99, dy = 99;
    quat_t q = quat_y(0.3f);
    TEST_ASSERT_EQUAL_INT(AG_OK, srv_motion_update(&q, 0.01f, &dx, &dy));
    TEST_ASSERT_EQUAL_INT(0, (int)dx);
    TEST_ASSERT_EQUAL_INT(0, (int)dy);
}

/* 4. Feeding the same quaternion twice yields zero output on the second call. */
static void test_identical_q_returns_zero(void)
{
    int8_t dx, dy;
    quat_t q = quat_y(0.3f);
    srv_motion_update(&q, 0.01f, &dx, &dy);          /* cache */
    srv_motion_update(&q, 0.01f, &dx, &dy);          /* delta is identity */
    TEST_ASSERT_EQUAL_INT(0, (int)dx);
    TEST_ASSERT_EQUAL_INT(0, (int)dy);
}

/* 5. Angular delta below `deadzone_rad` produces zero output. */
static void test_below_deadzone_returns_zero(void)
{
    prime_with_identity();
    int8_t dx, dy;
    /* Default deadzone = 0.02 rad; pick 0.01 (well below). */
    quat_t q = quat_y(0.01f);
    srv_motion_update(&q, 0.01f, &dx, &dy);
    TEST_ASSERT_EQUAL_INT(0, (int)dx);
    TEST_ASSERT_EQUAL_INT(0, (int)dy);
}

/* 6. Increasing |theta| never decreases |dx|. */
static void test_doubling_input_is_monotonic(void)
{
    int prev_abs = -1;
    /* Sweep well above deadzone, stay below the velocity cap
     * (400 * 0.3 + 0.3^1.6 ≈ 120 < 127). */
    for (float theta = 0.03f; theta <= 0.30f; theta += 0.02f) {
        motion_config_t c = cfg_default();
        srv_motion_init(&c);
        prime_with_identity();

        int8_t dx, dy;
        quat_t q = quat_y(theta);
        srv_motion_update(&q, 0.01f, &dx, &dy);
        const int curr = (dx < 0) ? -(int)dx : (int)dx;
        TEST_ASSERT_TRUE_MESSAGE(curr >= prev_abs,
                                 "|dx| decreased as |theta| increased");
        prev_abs = curr;
    }
}

/* 7. Output sign follows input sign for both axes. */
static void test_sign_preservation(void)
{
    int8_t dx, dy;

    /* Positive pitch → positive dx. */
    prime_with_identity();
    quat_t qpp = quat_y(0.10f);
    srv_motion_update(&qpp, 0.01f, &dx, &dy);
    TEST_ASSERT_TRUE_MESSAGE(dx > 0, "positive pitch should yield positive dx");

    /* Negative pitch → negative dx. */
    srv_motion_reset(); prime_with_identity();
    quat_t qpn = quat_y(-0.10f);
    srv_motion_update(&qpn, 0.01f, &dx, &dy);
    TEST_ASSERT_TRUE_MESSAGE(dx < 0, "negative pitch should yield negative dx");

    /* Positive yaw → negative dx (mirrored — combines with pitch). */
    srv_motion_reset(); prime_with_identity();
    quat_t qyp = quat_z(0.10f);
    srv_motion_update(&qyp, 0.01f, &dx, &dy);
    TEST_ASSERT_TRUE_MESSAGE(dx < 0, "positive yaw should yield negative dx");

    /* Positive roll → negative dy (dy is negated so hand-down = cursor-down). */
    srv_motion_reset(); prime_with_identity();
    quat_t qrp = quat_x(0.10f);
    srv_motion_update(&qrp, 0.01f, &dx, &dy);
    TEST_ASSERT_TRUE_MESSAGE(dy < 0, "positive roll should yield negative dy");

    /* Negative roll → positive dy. */
    srv_motion_reset(); prime_with_identity();
    quat_t qrn = quat_x(-0.10f);
    srv_motion_update(&qrn, 0.01f, &dx, &dy);
    TEST_ASSERT_TRUE_MESSAGE(dy > 0, "negative roll should yield positive dy");
}

/* 8. Engaging the clutch forces zero output; releasing restores it. */
static void test_clutch_zeros_output(void)
{
    prime_with_identity();
    int8_t dx, dy;

    srv_motion_set_clutch(true);
    quat_t q1 = quat_y(0.10f);
    srv_motion_update(&q1, 0.01f, &dx, &dy);
    TEST_ASSERT_EQUAL_INT(0, (int)dx);
    TEST_ASSERT_EQUAL_INT(0, (int)dy);

    /* Release clutch. Previous was updated under clutch, so feed a
     * fresh non-zero delta by rotating further. */
    srv_motion_set_clutch(false);
    quat_t q2 = quat_y(0.20f);
    srv_motion_update(&q2, 0.01f, &dx, &dy);
    TEST_ASSERT_TRUE_MESSAGE(dx != 0, "output should resume after clutch release");
}

/* 9. Extreme deltas saturate to the int8 range, never overflow. */
static void test_output_bounded_to_int8(void)
{
    int8_t dx, dy;

    prime_with_identity();
    quat_t qbig = quat_y(1.5f);   /* ~86 deg, far above cap */
    srv_motion_update(&qbig, 0.01f, &dx, &dy);
    TEST_ASSERT_TRUE(dx >= -127 && dx <= 127);
    TEST_ASSERT_TRUE(dy >= -127 && dy <= 127);

    srv_motion_reset(); prime_with_identity();
    quat_t qneg = quat_y(-1.5f);
    srv_motion_update(&qneg, 0.01f, &dx, &dy);
    TEST_ASSERT_TRUE(dx >= -127 && dx <= 127);
    TEST_ASSERT_TRUE(dy >= -127 && dy <= 127);
}

/* 10. After `srv_motion_reset`, the next call behaves like a first call. */
static void test_reset_reestablishes_first_frame(void)
{
    prime_with_identity();
    int8_t dx, dy;

    quat_t q = quat_y(0.10f);
    srv_motion_update(&q, 0.01f, &dx, &dy);
    TEST_ASSERT_TRUE_MESSAGE(dx != 0, "sanity: pre-reset should produce output");

    srv_motion_reset();
    srv_motion_update(&q, 0.01f, &dx, &dy);        /* first call post-reset */
    TEST_ASSERT_EQUAL_INT(0, (int)dx);
    TEST_ASSERT_EQUAL_INT(0, (int)dy);
}

/* 11. NULL pointer arguments are rejected. */
static void test_null_args_rejected(void)
{
    int8_t dx, dy;
    quat_t q = quat_y(0.10f);
    TEST_ASSERT_EQUAL_INT(AG_ERR_ARG, srv_motion_update(nullptr, 0.01f, &dx, &dy));
    TEST_ASSERT_EQUAL_INT(AG_ERR_ARG, srv_motion_update(&q, 0.01f, nullptr, &dy));
    TEST_ASSERT_EQUAL_INT(AG_ERR_ARG, srv_motion_update(&q, 0.01f, &dx, nullptr));
}

/* ── Entry point ──────────────────────────────────────────────────────── */

int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_init_rejects_null);
    RUN_TEST(test_init_rejects_bad_config);
    RUN_TEST(test_first_call_returns_zero);
    RUN_TEST(test_identical_q_returns_zero);
    RUN_TEST(test_below_deadzone_returns_zero);
    RUN_TEST(test_doubling_input_is_monotonic);
    RUN_TEST(test_sign_preservation);
    RUN_TEST(test_clutch_zeros_output);
    RUN_TEST(test_output_bounded_to_int8);
    RUN_TEST(test_reset_reestablishes_first_frame);
    RUN_TEST(test_null_args_rejected);
    return UNITY_END();
}
