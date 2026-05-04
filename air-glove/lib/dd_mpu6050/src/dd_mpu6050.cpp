/* dd_mpu6050 — MPU-6050 I2C driver (Phase I implementation).
 *
 * See docs/plans/02-dd-mpu6050.md for the step-by-step rationale, and
 * the register map in mpu6050_regs.h. ADR-001 locks in this chip and
 * ADR-005 permits the Arduino/Wire.h includes below because this is a
 * dd_* lib.
 *
 * All state is file-scope `static`; nothing is exported except the two
 * public entry points.
 */

#include <Arduino.h>
#include <Wire.h>
#include <esp_timer.h>
#include <math.h>

#include "dd_mpu6050.h"
#include "mpu6050_regs.h"
#include "ag_pins.h"

namespace {

constexpr float kGravity   = 9.80665f;                  /* m/s^2 per g          */
constexpr float kDegToRad  = 0.017453292519943295f;     /* pi / 180             */
constexpr uint8_t kBurstLen = 14;                       /* accel(6) + temp(2) + gyro(6) */

static float s_accel_scale_mps2 = 0.0f;
static float s_gyro_scale_rads  = 0.0f;
static bool  s_initialized      = false;

static ag_result_t write_reg(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(MPU6050_I2C_ADDR);
    Wire.write(reg);
    Wire.write(val);
    return (Wire.endTransmission() == 0) ? AG_OK : AG_ERR_IO;
}

static ag_result_t read_reg(uint8_t reg, uint8_t *out) {
    Wire.beginTransmission(MPU6050_I2C_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return AG_ERR_IO;
    if (Wire.requestFrom((uint8_t)MPU6050_I2C_ADDR, (uint8_t)1) != 1) return AG_ERR_IO;
    *out = (uint8_t)Wire.read();
    return AG_OK;
}

} /* namespace */

extern "C" ag_result_t dd_mpu6050_init(void) {
    Wire.begin(AG_PIN_I2C_SDA, AG_PIN_I2C_SCL);
    Wire.setClock(400000);

    uint8_t who = 0;
    if (read_reg(REG_WHO_AM_I, &who) != AG_OK) return AG_ERR_IO;
    if (who != 0x68) return AG_ERR_IO;

    /* Wake from sleep, select internal 8 MHz oscillator. */
    if (write_reg(REG_PWR_MGMT_1,   0x00) != AG_OK) return AG_ERR_IO;
    delay(2);

    /* DLPF_CFG = 1 → gyro 188 Hz / accel 184 Hz, 1 kHz internal rate.
     * Wider bandwidth cuts hardware phase lag (~1 ms vs ~10 ms at CFG=3),
     * giving the fusion filter fresher data and reducing cursor lag. */
    if (write_reg(REG_CONFIG,       0x01) != AG_OK) return AG_ERR_IO;

    /* FS_SEL = 1 → ±500 °/s → 65.5 LSB/(°/s). */
    if (write_reg(REG_GYRO_CONFIG,  0x08) != AG_OK) return AG_ERR_IO;

    /* AFS_SEL = 1 → ±4 g → 8192 LSB/g. */
    if (write_reg(REG_ACCEL_CONFIG, 0x08) != AG_OK) return AG_ERR_IO;

    /* Precompute "raw int16 → SI unit" scale factors. */
    s_accel_scale_mps2 = kGravity / 8192.0f;
    s_gyro_scale_rads  = kDegToRad / 65.5f;

    s_initialized = true;
    printf("[dd_mpu6050] WHO_AM_I=0x%02X  accel=+/-4g  gyro=+/-500dps  DLPF=188Hz  I2C=400kHz\n",
           (unsigned)who);
    return AG_OK;
}

extern "C" ag_result_t dd_mpu6050_read(imu_sample_t *out) {
    if (out == nullptr)  return AG_ERR_ARG;
    if (!s_initialized)  return AG_ERR_STATE;

    /* Pointer-then-burst: repeated start keeps the transaction atomic. */
    Wire.beginTransmission(MPU6050_I2C_ADDR);
    Wire.write(REG_ACCEL_XOUT_H);
    if (Wire.endTransmission(false) != 0) return AG_ERR_IO;

    if (Wire.requestFrom((uint8_t)MPU6050_I2C_ADDR, kBurstLen) != kBurstLen) {
        return AG_ERR_IO;
    }

    uint8_t b[kBurstLen];
    for (uint8_t i = 0; i < kBurstLen; ++i) {
        b[i] = (uint8_t)Wire.read();
    }

    /* Big-endian int16 pairs: accel XYZ, temperature (skipped), gyro XYZ. */
    const int16_t ax_raw = (int16_t)((uint16_t)b[0]  << 8 | b[1]);
    const int16_t ay_raw = (int16_t)((uint16_t)b[2]  << 8 | b[3]);
    const int16_t az_raw = (int16_t)((uint16_t)b[4]  << 8 | b[5]);
    /* b[6..7] = TEMP_OUT, unused in Phase I. */
    const int16_t gx_raw = (int16_t)((uint16_t)b[8]  << 8 | b[9]);
    const int16_t gy_raw = (int16_t)((uint16_t)b[10] << 8 | b[11]);
    const int16_t gz_raw = (int16_t)((uint16_t)b[12] << 8 | b[13]);

    out->ax   = (float)ax_raw * s_accel_scale_mps2;
    out->ay   = (float)ay_raw * s_accel_scale_mps2;
    out->az   = (float)az_raw * s_accel_scale_mps2;
    out->gx   = (float)gx_raw * s_gyro_scale_rads;
    out->gy   = (float)gy_raw * s_gyro_scale_rads;
    out->gz   = (float)gz_raw * s_gyro_scale_rads;
    out->t_us = (uint64_t)esp_timer_get_time();

    return AG_OK;
}
