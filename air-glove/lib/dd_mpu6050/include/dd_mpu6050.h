#ifndef DD_MPU6050_H
#define DD_MPU6050_H

#include "ag_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Initialise the MPU6050 over I2C. Must be called once before read.
 * Returns AG_OK, AG_ERR_IO (WHO_AM_I mismatch / bus error), or AG_ERR_INIT. */
ag_result_t dd_mpu6050_init(void);

/* Read one sample. Blocks briefly on I2C (< 500 us typical). Thread-safety:
 * callers must serialise (dedicated sampling task in `app_controller`). */
ag_result_t dd_mpu6050_read(imu_sample_t *out);

#ifdef __cplusplus
}
#endif
#endif /* DD_MPU6050_H */
