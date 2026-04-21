/* mpu6050_regs.h — MPU-6050 register addresses used by dd_mpu6050.
 *
 * Subset of the InvenSense MPU-6000/6050 Register Map (RM-MPU-6000A-00).
 * Private to this lib; not installed in include/. */

#ifndef MPU6050_REGS_H
#define MPU6050_REGS_H

#define MPU6050_I2C_ADDR    0x68   /* AD0 tied low on the AirGlove BOM  */

#define REG_PWR_MGMT_1      0x6B   /* bit 6 = SLEEP, bits [2:0] = CLKSEL */
#define REG_CONFIG          0x1A   /* bits [2:0] = DLPF_CFG              */
#define REG_GYRO_CONFIG     0x1B   /* bits [4:3] = FS_SEL                */
#define REG_ACCEL_CONFIG    0x1C   /* bits [4:3] = AFS_SEL               */
#define REG_ACCEL_XOUT_H    0x3B   /* start of 14-byte burst             */
#define REG_WHO_AM_I        0x75   /* expected value 0x68                */

#endif /* MPU6050_REGS_H */
