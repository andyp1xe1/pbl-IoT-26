#pragma once
/* srv_motion — quaternion-to-cursor motion mapping service.
 *
 * Consumes the unit quaternion stream produced by srv_fusion, computes the
 * frame-to-frame rotation delta, and emits int8 cursor deltas (dx, dy)
 * compatible with hid_mouse_report_t. Rate-control semantics: glove tilt
 * magnitude → cursor speed. Features: per-axis dead-zone, non-linear gain
 * curve, velocity cap, and a clutch gate that forces zero output.
 *
 * Thread-safety: NOT thread-safe. Call exclusively from t_motion task.
 * Platform:      Pure C++ (stdlib only). No Arduino or ESP-IDF includes.
 */

#include "ag_types.h"
#include "srv_fusion.h"   /* for quat_t */

#ifdef __cplusplus
extern "C" {
#endif

/* Tuning knobs. Values passed by the caller at init time. */
typedef struct {
    /* Magnitude of per-frame angular delta, in radians, below which the
     * output is forced to zero. Kills idle-hand tremor. Must be >= 0. */
    float deadzone_rad;

    /* Slope of the linear term in the gain curve:
     *     |out| = gain_low * |theta| + |theta|^gain_exp
     * in int8 units per radian of angular delta. Must be > 0. */
    float gain_low;

    /* Exponent of the non-linear term in the gain curve. Must be >= 1.0
     * so the curve is strictly non-decreasing in |theta|. Values close
     * to 1.0 give near-linear behaviour; larger values accelerate big
     * motions relative to small ones. */
    float gain_exp;

    /* Absolute cap applied before the final int8 clamp; must be > 0 and
     * typically <= 127. */
    float velocity_cap;

    /* Independent Y-axis output multiplier applied after the gain curve.
     * Use > 1.0 when up/down feels slower than left/right for the same hand
     * motion (common — wrist roll produces smaller angular deltas than pitch).
     * Must be > 0. 1.0 = no adjustment (symmetric axes). */
    float gain_y_scale;
} motion_config_t;

/* Initialise (or re-initialise) the motion mapper. Copies `*cfg` into the
 * internal state, clears the cached previous quaternion, and releases the
 * clutch. Returns AG_OK, or AG_ERR_ARG if `cfg` is NULL or any field is
 * non-finite / out of range (see `motion_config_t` for the per-field
 * constraints). */
ag_result_t srv_motion_init(const motion_config_t *cfg);

/* Consume one orientation quaternion and emit signed 8-bit cursor deltas.
 *
 * q     Pointer to a unit quaternion. Not modified.
 * dt_s  Seconds since the previous call (producer's responsibility). Only
 *       used as a safety guard; the mapping itself works in angular-delta
 *       domain and is independent of dt.
 * dx    Output: cursor X delta in [-127, +127].
 * dy    Output: cursor Y delta in [-127, +127].
 *
 * Behaviour:
 *   - First call after init / reset caches `*q`, writes dx=dy=0, returns AG_OK.
 *   - If clutch is active, writes dx=dy=0 AND updates the cached quaternion
 *     (so releasing the clutch does not cause a jump), returns AG_OK.
 *   - If dt_s is non-finite or < 1e-4 s, writes zeros and updates cache.
 *   - Otherwise computes q_delta = q_prev^-1 * q, takes twice the vector
 *     part as the rotation-vector approximation, applies per-axis deadzone,
 *     gain curve, velocity cap, and int8 clamp. Updates the cache on exit.
 *
 * Returns AG_OK on success, AG_ERR_ARG on NULL input. Never AG_ERR_STATE:
 * the mapper is usable immediately after init. */
ag_result_t srv_motion_update(const quat_t *q, float dt_s, int8_t *dx, int8_t *dy);

/* Engage (true) or release (false) the clutch gate. While engaged,
 * srv_motion_update writes zero output but continues to update the
 * cached quaternion so releasing does not produce a discontinuity. */
void srv_motion_set_clutch(bool active);

/* Invalidate the cached previous quaternion and release the clutch.
 * The next srv_motion_update becomes a "first call" that emits zero and
 * re-caches. The stored `motion_config_t` is preserved. */
void srv_motion_reset(void);

#ifdef __cplusplus
}
#endif
