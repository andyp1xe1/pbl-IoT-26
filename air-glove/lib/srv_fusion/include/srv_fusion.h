#ifndef SRV_FUSION_H
#define SRV_FUSION_H

#include "ag_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float q0, q1, q2, q3;
} quat_t;

/* Initialise the Madgwick filter. `beta` is the filter gain (default 0.08). */
ag_result_t srv_fusion_init(float beta);

/* Consume one IMU sample; emit the current unit quaternion.
 * `dt_s` is derived internally from `s->t_us` deltas. */
ag_result_t srv_fusion_update(const imu_sample_t *s, quat_t *out);

/* Reset internal quaternion to identity. */
void srv_fusion_reset(void);

#ifdef __cplusplus
}
#endif
#endif /* SRV_FUSION_H */
