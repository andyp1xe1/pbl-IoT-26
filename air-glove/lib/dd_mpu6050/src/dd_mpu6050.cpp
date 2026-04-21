/* dd_mpu6050 — stub until plan 02 lands. Returns deterministic errors so
 * callers don't mistake "unimplemented" for "working". */

#include "dd_mpu6050.h"

extern "C" ag_result_t dd_mpu6050_init(void) {
    return AG_ERR_INIT;
}

extern "C" ag_result_t dd_mpu6050_read(imu_sample_t *out) {
    (void)out;
    return AG_ERR_STATE;
}
