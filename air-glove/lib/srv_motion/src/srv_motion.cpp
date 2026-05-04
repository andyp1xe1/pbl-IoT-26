/* srv_motion — quaternion-to-cursor mapping.
 *
 * Algorithm (per docs/plans/06-srv-motion.md):
 *   1. delta = q_prev^-1 * q_curr  (conjugate for unit quaternion).
 *   2. Small-angle rotation vector ≈ 2 * delta.vec.
 *   3. Axis mapping: pitch (around Y) → dx, roll (around X) → dy.
 *   4. Per-axis dead-zone on |theta|.
 *   5. Gain curve:  mag = gain_low * |theta| + |theta|^gain_exp, sign from theta.
 *   6. Velocity cap, then clamp to int8.
 *
 * Pure C++ (stdlib only). NO Arduino / ESP-IDF includes.
 */

#include "srv_motion.h"
#include <math.h>

/* ── Internal state (~32 bytes SRAM) ───────────────────────────────────── */

static motion_config_t s_cfg = {
    /* deadzone_rad */ 0.02f,
    /* gain_low     */ 400.0f,
    /* gain_exp     */ 1.6f,
    /* velocity_cap */ 127.0f,
};
static quat_t s_q_prev   = { 1.0f, 0.0f, 0.0f, 0.0f };
static bool   s_has_prev = false;
static bool   s_clutch   = false;
static float  s_dx_ema   = 0.0f;
static float  s_dy_ema   = 0.0f;

/* ── Helpers ──────────────────────────────────────────────────────────── */

static bool cfg_is_valid(const motion_config_t *cfg)
{
    if (!cfg) return false;
    if (!isfinite(cfg->deadzone_rad) || cfg->deadzone_rad < 0.0f) return false;
    if (!isfinite(cfg->gain_low)     || cfg->gain_low     <= 0.0f) return false;
    if (!isfinite(cfg->gain_exp)     || cfg->gain_exp     <  1.0f) return false;
    if (!isfinite(cfg->velocity_cap) || cfg->velocity_cap <= 0.0f) return false;
    if (!isfinite(cfg->gain_y_scale) || cfg->gain_y_scale <= 0.0f) return false;
    return true;
}

static int8_t clamp_to_int8(float v)
{
    if (v >  127.0f) v =  127.0f;
    if (v < -127.0f) v = -127.0f;
    return (int8_t)lrintf(v);
}

static float apply_gain(float theta, const motion_config_t *cfg)
{
    const float a   = fabsf(theta);
    /* mag = gain_low * |θ| + |θ|^gain_exp
     * strictly non-decreasing in |θ| given gain_low > 0, gain_exp >= 1. */
    const float mag = cfg->gain_low * a + powf(a, cfg->gain_exp);
    float out = copysignf(mag, theta);
    if (out >  cfg->velocity_cap) out =  cfg->velocity_cap;
    if (out < -cfg->velocity_cap) out = -cfg->velocity_cap;
    return out;
}

/* ── Public API ───────────────────────────────────────────────────────── */

extern "C" ag_result_t srv_motion_init(const motion_config_t *cfg)
{
    if (!cfg_is_valid(cfg)) return AG_ERR_ARG;
    s_cfg      = *cfg;
    s_has_prev = false;
    s_clutch   = false;
    s_dx_ema   = 0.0f;
    s_dy_ema   = 0.0f;
    return AG_OK;
}

extern "C" void srv_motion_set_clutch(bool active)
{
    s_clutch = active;
}

extern "C" void srv_motion_reset(void)
{
    s_has_prev = false;
    s_clutch   = false;
    s_dx_ema   = 0.0f;
    s_dy_ema   = 0.0f;
}

extern "C" ag_result_t srv_motion_update(const quat_t *q, float dt_s,
                                         int8_t *dx, int8_t *dy)
{
    if (!q || !dx || !dy) return AG_ERR_ARG;

    /* First frame after init/reset: cache and emit zero. */
    if (!s_has_prev) {
        s_q_prev   = *q;
        s_has_prev = true;
        *dx = 0; *dy = 0;
        return AG_OK;
    }

    /* Clutch active: zero output but keep the cache fresh so release
     * does not produce a jump. */
    if (s_clutch) {
        s_q_prev = *q;
        *dx = 0; *dy = 0;
        return AG_OK;
    }

    /* Safety guard — pathologically small or non-finite dt_s. The math
     * below uses angular delta directly, so this only shields against
     * upstream glitches leaking NaN/Inf. */
    if (!isfinite(dt_s) || dt_s < 1e-4f) {
        s_q_prev = *q;
        *dx = 0; *dy = 0;
        return AG_OK;
    }

    /* q_delta = q_prev^-1 ⊗ q_curr.  Unit-quaternion inverse = conjugate. */
    const float p0 =  s_q_prev.q0;
    const float p1 = -s_q_prev.q1;
    const float p2 = -s_q_prev.q2;
    const float p3 = -s_q_prev.q3;
    const float c0 = q->q0, c1 = q->q1, c2 = q->q2, c3 = q->q3;

    /* Hamilton product — vector part only (scalar and q_k unused). */
    const float d1 = p0*c1 + p1*c0 + p2*c3 - p3*c2;
    const float d2 = p0*c2 - p1*c3 + p2*c0 + p3*c1;

    /* Small-angle rotation-vector approximation: 2 * delta.vec. */
    const float dtheta_x = 2.0f * d1;   /* around X axis — roll  */
    const float dtheta_y = 2.0f * d2;   /* around Y axis — pitch */

    /* Axis mapping:
     *   pitch (dtheta_y) → cursor dx  (tilt forward/back  = left/right)
     *   roll  (dtheta_x) → cursor dy  (tilt left/right    = up/down, negated)
     * Negating dy so that tilting the hand down moves the cursor down,
     * matching the natural hand orientation on the desk. */
    float theta_for_x =  dtheta_y;
    float theta_for_y = -dtheta_x;

    /* Radial (circular) dead-zone — applied to the 2-D magnitude, not
     * per-axis independently. A square per-axis dead-zone kills diagonal
     * movement because both components can sit below the threshold even
     * when the total motion is significant. The circular version treats
     * all directions equally, and the (r - dz)/r rescale keeps the
     * transition smooth (no sudden jump from zero to nonzero). */
    const float r = sqrtf(theta_for_x * theta_for_x +
                          theta_for_y * theta_for_y);
    if (r < s_cfg.deadzone_rad) {
        theta_for_x = 0.0f;
        theta_for_y = 0.0f;
    } else {
        const float scale = (r - s_cfg.deadzone_rad) / r;
        theta_for_x *= scale;
        theta_for_y *= scale;
    }

    const float out_x = apply_gain(theta_for_x, &s_cfg);
    const float out_y = apply_gain(theta_for_y, &s_cfg) * s_cfg.gain_y_scale;

    /* EMA on the float output before int8 quantisation.
     * Prevents single-pixel jitter when the motion is near an integer
     * boundary. alpha=0.22 → time constant ≈ 40 ms at 100 Hz — smooth
     * without being sluggish. Previous value (0.45, τ≈17 ms) was too short
     * to suppress jitter near quantisation boundaries. */
    static constexpr float kEma = 0.22f;
    s_dx_ema = kEma * out_x + (1.0f - kEma) * s_dx_ema;
    s_dy_ema = kEma * out_y + (1.0f - kEma) * s_dy_ema;

    *dx = clamp_to_int8(s_dx_ema);
    *dy = clamp_to_int8(s_dy_ema);

    s_q_prev = *q;
    return AG_OK;
}
