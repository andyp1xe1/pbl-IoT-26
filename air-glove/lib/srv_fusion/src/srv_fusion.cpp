/* srv_fusion — stub until plan 05 lands.
 * Pure C++: NO Arduino or ESP-IDF include may appear in this lib. */

#include "srv_fusion.h"

extern "C" ag_result_t srv_fusion_init(float beta) {
    (void)beta;
    return AG_OK;
}

extern "C" ag_result_t srv_fusion_update(const imu_sample_t *s, quat_t *out) {
    if (!s || !out) return AG_ERR_ARG;
    /* identity until the real filter lands */
    out->q0 = 1.0f;
    out->q1 = 0.0f;
    out->q2 = 0.0f;
    out->q3 = 0.0f;
    return AG_OK;
}

extern "C" void srv_fusion_reset(void) {
    /* no-op in stub */
}
