# E03 — IMU Driver (dd_mpu6050)

- **Status:** Not Started
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

- [ ] `dd_mpu6050_init()` returns `AG_OK` on a connected MPU6050 and `AG_ERR_IO` when the sensor is absent (unit power pulled).
- [ ] On-target test reads accel magnitude within `9.5 m/s² ≤ |a| ≤ 10.1 m/s²` at rest.
- [ ] On-target test reads gyro magnitude within `|ω| < 0.05 rad/s` at rest (bias characterisation; do not bias-correct in driver).
- [ ] `dd_mpu6050_read()` returns in < 500 µs per call (sampled with `esp_timer_get_time()`).
- [ ] Driver uses **no** static globals visible outside the lib (all state file-scope `static`).

## Dependencies

- E01, E02.

## Progress log

- 2026-04-21: Epic created. Pinout confirmed in `docs/srs/hardware.md`; plan in `docs/plans/02-dd-mpu6050.md`.
