# E03 — IMU Driver (dd_mpu6050)

- **Status:** In Progress (implementation complete; on-target acceptance pending hardware bring-up)
- **Phase:** I
- **Owns:** `air-glove/lib/dd_mpu6050/`
- **Plan:** `docs/plans/02-dd-mpu6050.md`
- **Realises:** FR-003

## Goal

Thin I²C wrapper for the MPU6050: init the bus, verify WHO_AM_I, read accel + gyro registers, scale to physical units, timestamp with `esp_timer_get_time()`. No fusion, no filtering — that's E06.

## Scope

**In:**
- I²C init on GPIO21 (SDA) / GPIO22 (SCL), 400 kHz.
- WHO_AM_I check (register 0x75 == 0x68).
- Basic configuration: wake from sleep, ±4 g accel range, ±500 °/s gyro range, DLPF_CFG = 3 (~44 Hz bandwidth — see plan 02 for the exact register map).
- `dd_mpu6050_read()` returns one `imu_sample_t` in m/s² and rad/s.
- On-target Unity test: WHO_AM_I, sample sanity.

**Out:**
- DMP (on-chip fusion) — we fuse in firmware per `docs/srs/decisions.md` ADR-002.
- FIFO mode — single-register reads are sufficient at 100 Hz.
- Magnetometer (MPU6050 has none).

## Public interface

See E02 (`dd_mpu6050.h`).

## Acceptance criteria

- [~] `dd_mpu6050_init()` returns `AG_OK` on a connected MPU6050 and `AG_ERR_IO` when the sensor is absent (unit power pulled). *Implementation returns `AG_ERR_IO` on WHO_AM_I mismatch, I2C NACK, or short burst. On-target verification pending.*
- [~] On-target test reads accel magnitude within `9.5 m/s² ≤ |a| ≤ 10.1 m/s²` at rest. *Test `test_gravity_magnitude_at_rest` written in `test/test_dd_mpu6050/test_main.cpp`. Pending hardware run.*
- [~] On-target test reads gyro magnitude within `|ω| < 0.05 rad/s` at rest. *Test `test_gyro_bias_at_rest` written. Pending hardware run.*
- [~] `dd_mpu6050_read()` returns in < 500 µs per call. *Test `test_read_latency_under_500us` written. Pending hardware run.*
- [x] Driver uses **no** static globals visible outside the lib (all state file-scope `static`). *Verified by `nm dd_mpu6050.o`: only `dd_mpu6050_init` and `dd_mpu6050_read` are exported; `s_accel_scale_mps2`, `s_gyro_scale_rads`, `s_initialized`, `write_reg`, `read_reg` are file-scope static.*

## Dependencies

- E01, E02.

## Progress log

- 2026-04-21: Epic created. Pinout confirmed in `docs/srs/hardware.md`; plan in `docs/plans/02-dd-mpu6050.md`.
- 2026-04-21: Implementation delivered. New files:
  - `air-glove/lib/dd_mpu6050/src/mpu6050_regs.h` — register addresses (MPU6050_I2C_ADDR, REG_PWR_MGMT_1, REG_CONFIG, REG_GYRO_CONFIG, REG_ACCEL_CONFIG, REG_ACCEL_XOUT_H, REG_WHO_AM_I).
  - `air-glove/lib/dd_mpu6050/src/dd_mpu6050.cpp` — full driver: WHO_AM_I check, config registers (DLPF_CFG=3, FS_SEL=1 → ±500 °/s, AFS_SEL=1 → ±4 g), 14-byte burst read at REG_ACCEL_XOUT_H, big-endian int16 decode, SI-unit scaling (m/s² via 9.80665/8192, rad/s via (π/180)/65.5), `esp_timer_get_time()` timestamp.
  - `air-glove/test/test_dd_mpu6050/test_main.cpp` — six on-target Unity tests (raw WHO_AM_I probe, `dd_mpu6050_init` OK, sample populates timestamp, gravity magnitude window, gyro bias bound, read latency < 500 µs).
  - `platformio.ini` `env:native` gained `test_ignore = test_dd_*` so the on-target test is skipped in host builds.
- 2026-04-21: Verification done locally — shim compile of `dd_mpu6050.cpp` with `g++ -Wall -Wextra -Werror` passes. `nm` confirms only the two public `extern "C"` symbols are exported; all helpers and state are file-scope `static`. Hardware bring-up (`pio test -e esp32dev -f test_dd_mpu6050` on a connected board) is the outstanding gate before marking the epic Done.
