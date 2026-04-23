# E06 — Sensor Fusion Service (srv_fusion)

- **Status:** In Progress (native tests all green; on-target `< 2 ms / update` bench on ESP32 is the remaining gate)
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

- [x] Native test: static input (`a = (0, 0, 9.81)`, `ω = 0`) converges to identity quaternion (`|1 − q0| < 0.01`) within 2 s of simulated time at dt = 10 ms. *Verified 2026-04-23: `test_identity_convergence_under_gravity` passes locally on g++ 9.2 via mini-Unity harness.*
- [x] Native test: quaternion norm stays in `[0.99, 1.01]` across 10 000 updates with Gaussian noise injection. *Verified 2026-04-23: `test_quaternion_norm_stable_under_noise` passes locally.*
- [x] Native test: single update runs in < 200 µs on the test host. *Verified 2026-04-23: `test_update_runs_under_200us_host` passes locally.*
- [ ] On-target benchmark (logged once in bring-up): `srv_fusion_update()` < 2 ms on ESP32 @ 240 MHz.
- [x] No Arduino or ESP-IDF include present — confirmed by `env:native` building the lib. *Verified: `srv_fusion.cpp` includes only `<math.h>` and `srv_fusion.h`; header includes only `ag_types.h`. Zero platform headers reachable.*

## Dependencies

- E02.

## Progress log

- 2026-04-21: Epic created. Madgwick chosen per `docs/srs/decisions.md` ADR-002. Plan: `docs/plans/05-srv-fusion.md`.
- 2026-04-21: Implementation delivered on branch `feat/sensor-fusion`. New / updated files:
  - `air-glove/lib/srv_fusion/library.json` — PlatformIO manifest (native + esp32dev, depends on app_config).
  - `air-glove/lib/srv_fusion/include/srv_fusion.h` — switched to `#pragma once`; full contract comments added (units, thread-safety, free-fall behaviour, return codes).
  - `air-glove/lib/srv_fusion/src/srv_fusion.cpp` — Madgwick 2010 IMU variant, inlined. State: `s_beta`, `s_q`, `s_prev_t_us` (~24 bytes SRAM). dt clamped to [1 ms, 50 ms]; free-fall guard on `|a|² < 1e-6`; norm-collapse detection resets to identity and returns `AG_ERR_IO`. Only `<math.h>` included.
  - `air-glove/test/test_srv_fusion/test_main.cpp` — 7 native Unity tests replacing the placeholder: init OK, bad-beta rejection, gravity convergence (2 s), norm stability (10 000 noisy samples), free-fall safety, null-arg rejection, < 200 µs timing.
- 2026-04-23: Host verification — built `srv_fusion.cpp` + `test_main.cpp` with g++ 9.2 through a mini-Unity shim. **7/7 tests pass, 0 failures.** All three native acceptance criteria flip to `[x]`. On-target ESP32 benchmark (`< 2 ms per update` at 240 MHz) is the remaining gate before Done. Minor build note: the tests use `quat_t q = {0};` which trips `-Wmissing-field-initializers`; benign under PIO's default `-Wall -Wextra` (no `-Werror`), fix by using `{}` or explicit four-field brace init if `-Werror` ever gets turned on.
