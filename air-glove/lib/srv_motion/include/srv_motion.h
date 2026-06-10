#pragma once
/* srv_motion — configurable mix-matrix cursor mapping.
 *
 * Consumes a 9-element signal vector built by t_motion (six raw IMU axes
 * plus three Madgwick-fused angular rates) and produces signed 8-bit cursor
 * deltas via:
 *
 *   x_raw = Σ mix_x_milli[i] · signal[i] / 1000
 *   y_raw = Σ mix_y_milli[i] · signal[i] / 1000
 *   apply sensitivity, radial deadzone, fixed gain curve, EMA, clamp to int8.
 *
 * Weight sign chooses contribution direction; magnitude chooses contribution
 * strength; zero disables an axis entirely. The fused-rate entries multiply
 * against zero whenever madgwick_enabled is off at the caller, so the same
 * pipeline serves both raw-IMU and fused-orientation use cases.
 *
 * Thread-safety: NOT thread-safe. Call exclusively from t_motion task.
 * Platform:      Pure C++ (stdlib only). No Arduino or ESP-IDF includes.
 */

#include "ag_types.h"   /* AG_MIX_COUNT, AG_MIX_* enum values */

#ifdef __cplusplus
extern "C" {
#endif

/* Tuning knobs taken straight from the companion-app config so the wire
 * format is the source of truth for ranges. Internal gain-curve constants
 * stay in srv_motion.cpp; users tune feel via sens_*_milli and the matrix. */
typedef struct {
    int16_t  mix_x_milli[AG_MIX_COUNT];
    int16_t  mix_y_milli[AG_MIX_COUNT];
    uint16_t sens_x_milli;     /* ×1000 multiplier on x_raw */
    uint16_t sens_y_milli;
    float    deadzone_rad;     /* radial deadzone after sens */
} motion_config_t;

/* Initialise (or re-initialise) the motion mapper. Copies `*cfg` into the
 * internal state and clears the EMA cache. Returns AG_OK, or AG_ERR_ARG if
 * `cfg` is NULL or sens_*_milli is zero. Mix-weight validation is delegated
 * to the codec; clamping happens on the wire side. */
ag_result_t srv_motion_init(const motion_config_t *cfg);

/* Mix one signal vector into a cursor delta. `signals` is indexed by
 * AG_MIX_* (raw gx,gy,gz in rad/s; raw ax,ay,az in m/s²; fused roll/pitch/
 * yaw rates in rad per call interval). Caller fills the fused entries with
 * zeros when Madgwick is disabled. Returns AG_OK on success, AG_ERR_ARG on
 * NULL input. Never AG_ERR_STATE. */
ag_result_t srv_motion_update(const float signals[AG_MIX_COUNT],
                              int8_t *dx, int8_t *dy);

/* Engage (true) or release (false) the clutch gate. While engaged,
 * srv_motion_update writes zero output and resets the EMA so releasing
 * does not produce a discontinuity. */
void srv_motion_set_clutch(bool active);

/* Drop the EMA cache and release the clutch. Preserves the stored config. */
void srv_motion_reset(void);

#ifdef __cplusplus
}
#endif
