#pragma once
/* srv_fusion — Madgwick 6-axis IMU sensor fusion service.
 *
 * Converts a stream of imu_sample_t (accel + gyro in SI units) into a
 * unit quaternion representing glove orientation relative to world frame.
 *
 * Thread-safety: NOT thread-safe. Call exclusively from t_fusion task.
 * Platform:      Pure C++ (stdlib only). No Arduino or ESP-IDF includes.
 */

#include "ag_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Unit quaternion: q = q0 + q1*i + q2*j + q3*k.
 * Invariant: q0^2 + q1^2 + q2^2 + q3^2 == 1.0 (maintained by srv_fusion_update). */
typedef struct {
    float q0, q1, q2, q3;
} quat_t;

/* Initialise (or re-initialise) the Madgwick filter.
 *
 * beta   Filter gain in [0, 1]. Controls convergence speed vs. noise
 *        rejection. Default: 0.08 (good for 100 Hz, ±4 g, ±500 °/s).
 *        Larger beta → faster convergence, more accel noise passed through.
 *
 * Resets the internal quaternion to identity (1, 0, 0, 0) and clears the
 * previous-timestamp register so the first call to srv_fusion_update uses
 * the default dt of 10 ms.
 *
 * Returns AG_OK, or AG_ERR_ARG if beta is outside [0, 1]. */
ag_result_t srv_fusion_init(float beta);

/* Consume one IMU sample and emit the current unit quaternion.
 *
 * s    Pointer to a valid imu_sample_t. ax/ay/az in m/s²; gx/gy/gz in rad/s;
 *      t_us in microseconds (monotonic, from esp_timer_get_time or equivalent).
 * out  Destination for the updated quaternion.
 *
 * dt is derived from consecutive s->t_us values (clamped to [1 ms, 50 ms]).
 * The first call after init/reset uses dt = 10 ms (assumes 100 Hz).
 *
 * Free-fall guard: if |a|² < 1e-6 the accel correction is skipped and the
 * filter runs gyro-only integration for that frame.
 *
 * Returns AG_OK on success, AG_ERR_ARG on null pointer, or AG_ERR_IO if the
 * quaternion norm collapses to a non-finite value (state is auto-reset to
 * identity in that case). */
ag_result_t srv_fusion_update(const imu_sample_t *s, quat_t *out);

/* Reset the internal quaternion to identity (1, 0, 0, 0) without changing
 * the stored beta or the previous-timestamp register. */
void srv_fusion_reset(void);

#ifdef __cplusplus
}
#endif
