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

/* Toggle the MPU6050 SLEEP bit (PWR_MGMT_1[6]). When asleep the chip draws
 * ~5 µA instead of ~3.6 mA, gyro/accel outputs are frozen, and reads via
 * dd_mpu6050_read() return stale data. DLPF / FS_SEL / AFS_SEL registers are
 * preserved across sleep, so wake-up needs no reconfiguration. Caller must
 * serialise with dd_mpu6050_read() (same task). */
ag_result_t dd_mpu6050_set_sleep(bool sleeping);

/* Gyro bias correction (body-frame, rad/s). Subtracted from every
 * dd_mpu6050_read() result after the axis remap. Loaded from NVS at init and
 * applied immediately; callers can update it at runtime (e.g. after a hold-
 * still calibration) and optionally persist it.
 *
 * Storage is body-frame (post-remap) so a calibration captures whatever the
 * caller sees on the output — re-calibrate if the axis remap ever changes. */
void        dd_mpu6050_set_gyro_bias (float bx, float by, float bz);
void        dd_mpu6050_get_gyro_bias (float *bx, float *by, float *bz);
ag_result_t dd_mpu6050_save_gyro_bias(void);

#ifdef __cplusplus
}
#endif
#endif /* DD_MPU6050_H */
