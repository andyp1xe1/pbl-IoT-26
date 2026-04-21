/* dd_mpu6050.h — public interface of the MPU6050 I2C IMU driver.
 *
 * Layer: dd (device driver). May be implemented with Arduino/Wire.h.
 * Consumers (`srv_fusion` via a FreeRTOS queue) treat this as a pure
 * producer of `imu_sample_t` values.
 *
 * Implementation lives in plan 02 (`docs/plans/02-dd-mpu6050.md`).
 */

#ifndef DD_MPU6050_H
#define DD_MPU6050_H

#include "ag_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Initialise the MPU6050 over I2C. Must be called exactly once before
 * any `dd_mpu6050_read()`. Not thread-safe; call from a single task at
 * boot (typically `app_controller_start()`).
 *
 * Returns AG_OK on success; AG_ERR_IO on bus error or WHO_AM_I mismatch;
 * AG_ERR_INIT on internal configuration failure. */
ag_result_t dd_mpu6050_init(void);

/* Read one sample. Blocks briefly on I2C (< 500 us typical at 400 kHz).
 * Thread-safety: NOT safe for concurrent callers — serialise via the
 * dedicated `t_imu_sample` task documented in `docs/srs/architecture.md`.
 *
 * On AG_OK the fields of *out are fully written. On error *out is
 * left untouched; caller must inspect the return value. */
ag_result_t dd_mpu6050_read(imu_sample_t *out);

#ifdef __cplusplus
}
#endif
#endif /* DD_MPU6050_H */
