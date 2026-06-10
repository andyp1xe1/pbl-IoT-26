/* srv_motion — mix-matrix cursor mapping. See header for the contract.
 *
 * Internal gain curve and EMA constants are unchanged from the pre-mix
 * pipeline so default-weighted output reproduces the historical feel:
 *
 *   gain_low = 600  per unit of mixed input
 *   gain_exp = 1.2  (mildly accelerating large motions)
 *   cap      = 127  (int8 saturation)
 *   ema      = 0.22 (≈ 40 ms τ at 100 Hz)
 *
 * Pure C++ (stdlib only). NO Arduino / ESP-IDF includes.
 */

#include "srv_motion.h"
#include <math.h>

namespace {

motion_config_t s_cfg = {
    /* mix_x_milli  */ {   0,  -50, 0, 0,     0, -1000 },
    /* mix_y_milli  */ { +50,    0, 0, 0, +1000,     0 },
    /* sens_x_milli */ 1000,
    /* sens_y_milli */ 1000,
    /* deadzone_rad */ 0.015f,
};
bool  s_clutch = false;
float s_dx_ema = 0.0f;
float s_dy_ema = 0.0f;

constexpr float kGainLow = 600.0f;
constexpr float kGainExp = 1.2f;
constexpr float kCap     = 127.0f;
constexpr float kEma     = 0.22f;

inline int8_t clamp_to_int8(float v) {
    if (v >  127.0f) v =  127.0f;
    if (v < -127.0f) v = -127.0f;
    return (int8_t)lrintf(v);
}

inline float apply_gain(float v) {
    const float a   = fabsf(v);
    const float mag = kGainLow * a + powf(a, kGainExp);
    float out = copysignf(mag, v);
    if (out >  kCap) out =  kCap;
    if (out < -kCap) out = -kCap;
    return out;
}

}  // namespace

extern "C" ag_result_t srv_motion_init(const motion_config_t *cfg) {
    if (!cfg) return AG_ERR_ARG;
    if (cfg->sens_x_milli == 0 || cfg->sens_y_milli == 0) return AG_ERR_ARG;
    if (!isfinite(cfg->deadzone_rad) || cfg->deadzone_rad < 0.0f) return AG_ERR_ARG;
    s_cfg    = *cfg;
    s_clutch = false;
    s_dx_ema = 0.0f;
    s_dy_ema = 0.0f;
    return AG_OK;
}

extern "C" void srv_motion_set_clutch(bool active) {
    s_clutch = active;
    if (active) {
        s_dx_ema = 0.0f;
        s_dy_ema = 0.0f;
    }
}

extern "C" void srv_motion_reset(void) {
    s_clutch = false;
    s_dx_ema = 0.0f;
    s_dy_ema = 0.0f;
}

extern "C" ag_result_t srv_motion_update(const float signals[AG_MIX_COUNT],
                                         int8_t *dx, int8_t *dy) {
    if (!signals || !dx || !dy) return AG_ERR_ARG;

    if (s_clutch) {
        *dx = 0;
        *dy = 0;
        return AG_OK;
    }

    /* Linear mix. milli-scaled weights → divide once after the sum. */
    float x_raw = 0.0f, y_raw = 0.0f;
    for (int i = 0; i < AG_MIX_COUNT; ++i) {
        x_raw += (float)s_cfg.mix_x_milli[i] * signals[i];
        y_raw += (float)s_cfg.mix_y_milli[i] * signals[i];
    }
    x_raw *= 0.001f;
    y_raw *= 0.001f;

    x_raw *= (float)s_cfg.sens_x_milli * 0.001f;
    y_raw *= (float)s_cfg.sens_y_milli * 0.001f;

    /* Radial deadzone in the mixed plane. Square per-axis would kill
     * diagonals; the (r-dz)/r rescale keeps the transition smooth. */
    const float r = sqrtf(x_raw * x_raw + y_raw * y_raw);
    if (r < s_cfg.deadzone_rad) {
        x_raw = 0.0f;
        y_raw = 0.0f;
    } else if (r > 0.0f) {
        const float scale = (r - s_cfg.deadzone_rad) / r;
        x_raw *= scale;
        y_raw *= scale;
    }

    const float out_x = apply_gain(x_raw);
    const float out_y = apply_gain(y_raw);

    s_dx_ema = kEma * out_x + (1.0f - kEma) * s_dx_ema;
    s_dy_ema = kEma * out_y + (1.0f - kEma) * s_dy_ema;

    *dx = clamp_to_int8(s_dx_ema);
    *dy = clamp_to_int8(s_dy_ema);
    return AG_OK;
}
