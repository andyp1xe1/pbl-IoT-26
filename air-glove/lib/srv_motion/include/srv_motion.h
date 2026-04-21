#ifndef SRV_MOTION_H
#define SRV_MOTION_H

#include "ag_types.h"
#include "srv_fusion.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float deadzone_rad;
    float gain_low;
    float gain_exp;
    float velocity_cap;
} motion_config_t;

ag_result_t srv_motion_init(const motion_config_t *cfg);

/* Consume one orientation quaternion; emit signed 8-bit cursor deltas.
 * dt_s is the time since the previous call. */
ag_result_t srv_motion_update(const quat_t *q, float dt_s, int8_t *dx, int8_t *dy);

void srv_motion_set_clutch(bool active);
void srv_motion_reset(void);

#ifdef __cplusplus
}
#endif
#endif /* SRV_MOTION_H */
