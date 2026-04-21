# E06 — Sensor Fusion Service (srv_fusion)

- **Status:** Not Started
- **Phase:** I
- **Owns:** `air-glove/lib/srv_fusion/`
- **Plan:** `docs/plans/05-srv-fusion.md`
- **Realises:** FR-003, NFR-STAB-001, NFR-LAT-001 (processing half)

## Goal

Implement the Madgwick 6-axis IMU filter to turn `imu_sample_t` (accel + gyro) into a stable unit quaternion representing glove orientation. Pure C — no Arduino/ESP-IDF includes — so it is unit-testable on the host `env:native`.

## Scope

**In:**
- Madgwick IMU variant (no magnetometer — MPU6050 has none).
- Configurable gain β (default 0.08, tunable through init).
- 100 Hz update rate driven by `t_fusion` consuming `q_imu`.
- Numerical hygiene: normalise quaternion each step; guard divide-by-zero on zero-accel (free fall) frames.
- Native unit tests in `test/test_srv_fusion/`: identity convergence, bounded drift under pure-noise input, throughput microbench.

**Out:**
- Motion-to-cursor mapping — E07.
- Calibration/bias persistence — E12 (backlog).

## Public interface

```c
// srv_fusion/include/srv_fusion.h
#include "ag_types.h"

typedef struct { float q0, q1, q2, q3; } quat_t;

ag_result_t srv_fusion_init(float beta);
ag_result_t srv_fusion_update(const imu_sample_t *s, quat_t *out);
void        srv_fusion_reset(void);
```

## Acceptance criteria

- [ ] Native test: static input (`a = (0, 0, 9.81)`, `ω = 0`) converges to identity quaternion (`|1 − q0| < 0.01`) within 2 s of simulated time at dt = 10 ms.
- [ ] Native test: quaternion norm stays in `[0.99, 1.01]` across 10 000 updates with Gaussian noise injection.
- [ ] Native test: single update runs in < 200 µs on the test host.
- [ ] On-target benchmark (logged once in bring-up): `srv_fusion_update()` < 2 ms on ESP32 @ 240 MHz.
- [ ] No Arduino or ESP-IDF include present — confirmed by `env:native` building the lib.

## Dependencies

- E02.

## Progress log

- 2026-04-21: Epic created. Madgwick chosen per `docs/srs/decisions.md` ADR-002. Plan: `docs/plans/05-srv-fusion.md`.
