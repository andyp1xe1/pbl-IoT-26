/* srv_fusion — Madgwick 2010 IMU variant (6-axis: accel + gyro).
 *
 * Reference: Madgwick, S., "An efficient orientation filter for inertial
 * and inertial/magnetic sensor arrays," 2010. IMU-only (no magnetometer)
 * gradient-descent formulation, inlined — no third-party filter library.
 *
 * Pure C++ (stdlib only). NO Arduino or ESP-IDF includes may appear here.
 */

#include "srv_fusion.h"
#include <math.h>

/* ── Internal state (~24 bytes SRAM) ─────────────────────────────────────── */

static float    s_beta      = 0.08f;
static quat_t   s_q         = {1.0f, 0.0f, 0.0f, 0.0f};
static uint64_t s_prev_t_us = 0u;

/* ── Public API ───────────────────────────────────────────────────────────── */

extern "C" ag_result_t srv_fusion_init(float beta)
{
    if (beta < 0.0f || beta > 1.0f) {
        return AG_ERR_ARG;
    }
    s_beta      = beta;
    s_q         = {1.0f, 0.0f, 0.0f, 0.0f};
    s_prev_t_us = 0u;
    return AG_OK;
}

extern "C" void srv_fusion_reset(void)
{
    s_q = {1.0f, 0.0f, 0.0f, 0.0f};
    /* s_beta and s_prev_t_us are intentionally preserved */
}

extern "C" ag_result_t srv_fusion_update(const imu_sample_t *s, quat_t *out)
{
    if (!s || !out) {
        return AG_ERR_ARG;
    }

    /* ── 1. Compute dt ──────────────────────────────────────────────────── */
    float dt;
    if (s_prev_t_us == 0u) {
        dt = 0.01f;   /* first frame: assume 100 Hz */
    } else {
        dt = (float)(s->t_us - s_prev_t_us) * 1e-6f;
        /* Clamp to [1 ms, 50 ms] to survive queue stalls or clock wraps */
        if (dt < 0.001f) dt = 0.001f;
        if (dt > 0.050f) dt = 0.050f;
    }
    s_prev_t_us = s->t_us;

    /* ── 2. Load locals ─────────────────────────────────────────────────── */
    float q0 = s_q.q0, q1 = s_q.q1, q2 = s_q.q2, q3 = s_q.q3;
    float ax = s->ax,  ay = s->ay,  az = s->az;
    float gx = s->gx,  gy = s->gy,  gz = s->gz;

    /* ── 3. Quaternion derivative from gyro (qDot = 0.5 * q ⊗ ω_pure) ──── */
    float qDot0 = 0.5f * (-q1 * gx - q2 * gy - q3 * gz);
    float qDot1 = 0.5f * ( q0 * gx + q2 * gz - q3 * gy);
    float qDot2 = 0.5f * ( q0 * gy - q1 * gz + q3 * gx);
    float qDot3 = 0.5f * ( q0 * gz + q1 * gy - q2 * gx);

    /* ── 4. Madgwick accel correction (skip during free-fall) ───────────── *
     * Madgwick 2010 IMU variant, gradient descent, 6-axis.
     * Objective function: f = q^T * g_ref - a_body  where g_ref = (0,0,1)
     * after accel normalisation. The Jacobian-transposed gradient gradF is
     * computed analytically from the 3 residuals and the 4 quaternion
     * components. */
    float accel_sq = ax * ax + ay * ay + az * az;
    if (accel_sq > 1e-6f) {
        /* Normalise accelerometer measurement */
        float recip = 1.0f / sqrtf(accel_sq);
        ax *= recip;
        ay *= recip;
        az *= recip;

        /* Pre-computed products reused across gradient terms */
        float _2q0 = 2.0f * q0;
        float _2q1 = 2.0f * q1;
        float _2q2 = 2.0f * q2;
        float _2q3 = 2.0f * q3;
        float _4q0 = 4.0f * q0;
        float _4q1 = 4.0f * q1;
        float _4q2 = 4.0f * q2;
        float _8q1 = 8.0f * q1;
        float _8q2 = 8.0f * q2;
        float q0q0 = q0 * q0;
        float q1q1 = q1 * q1;
        float q2q2 = q2 * q2;
        float q3q3 = q3 * q3;

        /* Gradient of objective function f(q) = q^T * g_ref - a_body
         * Analytical Jacobian^T * f evaluated at current estimate: */
        float s0 = _4q0 * q2q2 + _2q2 * ax + _4q0 * q1q1 - _2q1 * ay;
        float s1 = _4q1 * q3q3 - _2q3 * ax + 4.0f * q0q0 * q1 - _2q0 * ay
                   - _4q1 + _8q1 * q1q1 + _8q1 * q2q2 + _4q1 * az;
        float s2 = 4.0f * q0q0 * q2 + _2q0 * ax + _4q2 * q3q3 - _2q3 * ay
                   - _4q2 + _8q2 * q1q1 + _8q2 * q2q2 + _4q2 * az;
        float s3 = 4.0f * q1q1 * q3 - _2q1 * ax + 4.0f * q2q2 * q3 - _2q2 * ay;

        /* Normalise gradient */
        float grad_norm = sqrtf(s0 * s0 + s1 * s1 + s2 * s2 + s3 * s3);
        if (grad_norm > 1e-10f) {
            float rg = 1.0f / grad_norm;
            s0 *= rg; s1 *= rg; s2 *= rg; s3 *= rg;
        }

        /* Subtract beta * gradF from the gyro derivative */
        qDot0 -= s_beta * s0;
        qDot1 -= s_beta * s1;
        qDot2 -= s_beta * s2;
        qDot3 -= s_beta * s3;
    }

    /* ── 5. Integrate: q += qDot * dt ──────────────────────────────────── */
    q0 += qDot0 * dt;
    q1 += qDot1 * dt;
    q2 += qDot2 * dt;
    q3 += qDot3 * dt;

    /* ── 6. Normalise quaternion ─────────────────────────────────────────── */
    float norm_sq = q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3;
    float recipNorm = 1.0f / sqrtf(norm_sq);

    if (!isfinite(recipNorm)) {
        /* Catastrophic collapse — reset to identity and report fault */
        s_q = {1.0f, 0.0f, 0.0f, 0.0f};
        *out = s_q;
        return AG_ERR_IO;
    }

    s_q.q0 = q0 * recipNorm;
    s_q.q1 = q1 * recipNorm;
    s_q.q2 = q2 * recipNorm;
    s_q.q3 = q3 * recipNorm;

    *out = s_q;
    return AG_OK;
}
