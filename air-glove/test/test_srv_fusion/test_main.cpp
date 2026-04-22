/* test_srv_fusion — Native Unity tests for the Madgwick sensor fusion service.
 *
 * Run with:  pio test -e native -f test_srv_fusion
 *
 * All tests operate on simulated input — no hardware required.
 */

#include <unity.h>
#include <math.h>
#include <stdint.h>
#include <time.h>

#include "srv_fusion.h"

void setUp(void) {
    /* Reset to a known state before every test */
    srv_fusion_init(0.08f);
}

void tearDown(void) {}

/* ── Helpers ──────────────────────────────────────────────────────────────── */

static imu_sample_t make_sample(float ax, float ay, float az,
                                float gx, float gy, float gz,
                                uint64_t t_us)
{
    imu_sample_t s;
    s.ax = ax; s.ay = ay; s.az = az;
    s.gx = gx; s.gy = gy; s.gz = gz;
    s.t_us = t_us;
    return s;
}

/* ── Tests ────────────────────────────────────────────────────────────────── */

/* 1. Init accepts the default gain */
static void test_init_returns_ok_default_beta(void)
{
    ag_result_t r = srv_fusion_init(0.08f);
    TEST_ASSERT_EQUAL_INT(AG_OK, r);
}

/* 2. Init rejects out-of-range gain */
static void test_init_rejects_bad_beta(void)
{
    TEST_ASSERT_EQUAL_INT(AG_ERR_ARG, srv_fusion_init(-0.1f));
    TEST_ASSERT_EQUAL_INT(AG_ERR_ARG, srv_fusion_init(2.0f));
    /* boundary values must be accepted */
    TEST_ASSERT_EQUAL_INT(AG_OK, srv_fusion_init(0.0f));
    TEST_ASSERT_EQUAL_INT(AG_OK, srv_fusion_init(1.0f));
}

/* 3. Gravity-only input converges toward identity (q0 → 1)
 *    200 iterations × 10 ms = 2 s of simulated time. */
static void test_identity_convergence_under_gravity(void)
{
    srv_fusion_init(0.08f);

    quat_t q = {0};
    imu_sample_t s = make_sample(0.0f, 0.0f, 9.81f,
                                 0.0f, 0.0f, 0.0f, 0u);

    for (int i = 0; i < 200; ++i) {
        s.t_us = (uint64_t)(i + 1) * 10000u;   /* 10 ms steps */
        ag_result_t r = srv_fusion_update(&s, &q);
        TEST_ASSERT_EQUAL_INT(AG_OK, r);
    }

    /* After 2 s at 100 Hz with pure gravity, q0 must be within 0.01 of 1 */
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, q.q0);
}

/* 4. Quaternion norm stays within [0.99, 1.01] under 10 000 noisy samples.
 *    Uses a simple deterministic pseudo-random noise (LCG) to stay portable. */
static void test_quaternion_norm_stable_under_noise(void)
{
    srv_fusion_init(0.08f);

    uint32_t rng = 12345u;   /* LCG seed */
    const float noise_scale = 0.1f;

    quat_t q = {0};
    uint64_t t = 0u;

    for (int i = 0; i < 10000; ++i) {
        /* LCG: produces values in [0, 0xFFFFFFFF] */
        rng = rng * 1664525u + 1013904223u;
        float na = ((float)(rng & 0xFFFFu) / 32767.5f - 1.0f) * noise_scale;
        rng = rng * 1664525u + 1013904223u;
        float nb = ((float)(rng & 0xFFFFu) / 32767.5f - 1.0f) * noise_scale;
        rng = rng * 1664525u + 1013904223u;
        float nc = ((float)(rng & 0xFFFFu) / 32767.5f - 1.0f) * noise_scale;

        t += 10000u;
        imu_sample_t s = make_sample(na, nb, 9.81f + nc,
                                     na * 0.01f, nb * 0.01f, nc * 0.01f, t);
        ag_result_t r = srv_fusion_update(&s, &q);
        TEST_ASSERT_EQUAL_INT(AG_OK, r);

        float norm = sqrtf(q.q0*q.q0 + q.q1*q.q1 + q.q2*q.q2 + q.q3*q.q3);
        TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, norm);
    }
}

/* 5. Free-fall frames (a = 0) must not crash and norm must remain finite */
static void test_freefall_frame_does_not_crash(void)
{
    srv_fusion_init(0.08f);
    quat_t q = {0};

    for (int i = 0; i < 10; ++i) {
        imu_sample_t s = make_sample(0.0f, 0.0f, 0.0f,
                                     0.0f, 0.0f, 0.0f,
                                     (uint64_t)(i + 1) * 10000u);
        ag_result_t r = srv_fusion_update(&s, &q);
        TEST_ASSERT_EQUAL_INT(AG_OK, r);

        float norm = sqrtf(q.q0*q.q0 + q.q1*q.q1 + q.q2*q.q2 + q.q3*q.q3);
        TEST_ASSERT_TRUE(isfinite(norm));
        TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, norm);
    }
}

/* 6. Null pointer arguments are rejected with AG_ERR_ARG */
static void test_null_args_rejected(void)
{
    imu_sample_t s = make_sample(0.0f, 0.0f, 9.81f, 0.0f, 0.0f, 0.0f, 10000u);
    quat_t q = {0};

    TEST_ASSERT_EQUAL_INT(AG_ERR_ARG, srv_fusion_update(nullptr, &q));
    TEST_ASSERT_EQUAL_INT(AG_ERR_ARG, srv_fusion_update(&s, nullptr));
}

/* 7. A single update must complete in < 200 µs on the test host.
 *    This is informational — it will soft-fail on extremely slow/loaded CI. */
static void test_update_runs_under_200us_host(void)
{
    srv_fusion_init(0.08f);
    imu_sample_t s = make_sample(0.0f, 0.0f, 9.81f,
                                 0.01f, 0.0f, 0.0f, 10000u);
    quat_t q = {0};

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    srv_fusion_update(&s, &q);
    clock_gettime(CLOCK_MONOTONIC, &t1);

    long elapsed_ns = (long)(t1.tv_sec - t0.tv_sec) * 1000000000L
                    + (t1.tv_nsec - t0.tv_nsec);

    /* 200 µs = 200 000 ns */
    TEST_ASSERT_LESS_THAN(200000L, elapsed_ns);
}

/* ── Entry point ──────────────────────────────────────────────────────────── */

int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    UNITY_BEGIN();

    RUN_TEST(test_init_returns_ok_default_beta);
    RUN_TEST(test_init_rejects_bad_beta);
    RUN_TEST(test_identity_convergence_under_gravity);
    RUN_TEST(test_quaternion_norm_stable_under_noise);
    RUN_TEST(test_freefall_frame_does_not_crash);
    RUN_TEST(test_null_args_rejected);
    RUN_TEST(test_update_runs_under_200us_host);

    return UNITY_END();
}
