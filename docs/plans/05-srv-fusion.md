# Plan 05 — srv_fusion (Madgwick 6-axis IMU filter)

- **Epic:** [E06](../../.claude/memory/epics/E06-sensor-fusion-service.md)
- **Goal:** Implement the Madgwick IMU (accel + gyro, no magnetometer) filter as a pure-C service that converts one `imu_sample_t` per call into a unit quaternion. No Arduino/ESP-IDF includes — the library must build and test under `env:native`.
- **Preconditions:** E02 types exist (`ag_result_t`, `imu_sample_t`, `AG_OK`, `AG_ERR_ARG`). Plan 01 has scaffolded `air-glove/lib/srv_fusion/` with `include/` and `src/` and a `library.json` that lists both `native` and `esp32dev` under `platforms`.

## Files to create

| Path (under `air-glove/lib/srv_fusion/`) | Role |
|---|---|
| `include/srv_fusion.h` | Public header (init/update/reset + `quat_t`). |
| `src/srv_fusion.cpp` | Madgwick IMU variant implementation (stdlib only). |
| `library.json` | Lib manifest, compatible with `native` and `esp32dev`. |
| `test/test_srv_fusion/test_main.cpp` | Native Unity tests (convergence + stability). |

## Step-by-step

1. In `srv_fusion.h`, declare the `quat_t` struct exactly as in the epic and the three functions verbatim. Include only `"ag_types.h"` — no `Arduino.h`, no `<FreeRTOS.h>`.
2. In `srv_fusion.cpp`, include `<math.h>` only. File-scope state:
   - `static float s_beta = 0.08f;`
   - `static quat_t s_q = { 1.0f, 0.0f, 0.0f, 0.0f };`
   - `static uint64_t s_prev_t_us = 0;`
3. Implement `srv_fusion_init(float beta)`:
   - If `beta < 0.0f || beta > 1.0f`, return `AG_ERR_ARG`.
   - `s_beta = beta; s_q = { 1, 0, 0, 0 }; s_prev_t_us = 0;`
   - Return `AG_OK`.
4. Implement `srv_fusion_reset(void)` — identical to the init reset block but does not touch `s_beta`.
5. Implement `srv_fusion_update(const imu_sample_t *s, quat_t *out)`:
   1. Validate `s != nullptr && out != nullptr` → else `AG_ERR_ARG`.
   2. Compute `dt` from timestamps: if `s_prev_t_us == 0`, `dt = 0.01f;` (first frame assumption of 100 Hz). Else `dt = (s->t_us - s_prev_t_us) * 1e-6f;`. Clamp `dt` to `[0.001f, 0.05f]` to survive queue stalls. `s_prev_t_us = s->t_us;`.
   3. Load locals `q0,q1,q2,q3` from `s_q`; `ax,ay,az` and `gx,gy,gz` from `*s`.
   4. If `(ax*ax + ay*ay + az*az) > 1e-6f` (not free-fall), run the Madgwick correction branch (see Internal design notes). Otherwise, gyro-only integration.
   5. Integrate: `qDot = 0.5 * q (*) (0, gx, gy, gz)`; subtract `beta * gradF` computed from the accel residual.
   6. `q += qDot * dt;`
   7. Normalise: `recipNorm = 1.0f / sqrtf(q0*q0 + q1*q1 + q2*q2 + q3*q3);` then multiply each component. If `recipNorm` is non-finite → re-init `s_q` to identity and return `AG_ERR_IO`.
   8. Store back into `s_q`; copy into `*out`.
   9. Return `AG_OK`.
6. The Madgwick 6-axis gradient is the classical formulation; inline it directly — DO NOT pull a third-party library. Tag the function comment with "Madgwick 2010 IMU variant, gradient descent, 6-axis".
7. Compile-check: `pio run -e native -d air-glove/lib/srv_fusion` must build without any platform header.

## Public header (target signature)

```c
// lib/srv_fusion/include/srv_fusion.h
#pragma once
#include "ag_types.h"

typedef struct { float q0, q1, q2, q3; } quat_t;

// Set filter gain beta (default 0.08 recommended). Resets internal
// quaternion to identity. Returns AG_ERR_ARG if beta is out of [0,1].
ag_result_t srv_fusion_init(float beta);

// Consume one IMU sample and produce one unit quaternion.
// dt is derived from consecutive s->t_us (clamped to [1ms, 50ms]).
// Returns AG_OK, AG_ERR_ARG on null input, or AG_ERR_IO if the
// quaternion norm collapses (state is auto-reset in that case).
ag_result_t srv_fusion_update(const imu_sample_t *s, quat_t *out);

// Reset quaternion to identity without touching the stored beta.
void        srv_fusion_reset(void);
```

## Internal design notes

Madgwick IMU variant, inlined. Given `(ax, ay, az)` normalised and current estimate `q = (q0, q1, q2, q3)`, the objective function is `f = qT * g_ref - a_body`, where `g_ref = (0, 0, 1)` (after accel normalisation). Its Jacobian produces the gradient `gradF` in quaternion space. Standard steps per update:

1. Normalise `a` (skip if `|a|^2` < epsilon → free-fall).
2. Compute `gradF` from the 3 residuals + 4 quaternion components (constants `2*q0 etc.` are reused — keep them as locals).
3. `gradF /= |gradF|`.
4. Quaternion derivative from gyro: `qDot = 0.5 * q (*) (0, gx, gy, gz)`, then `qDot -= beta * gradF`.
5. Integrate `q += qDot * dt`; renormalise.

No dynamic allocation, no statics beyond the quaternion and beta and last-timestamp. Total SRAM footprint: ~24 bytes.

At 100 Hz update rate the Madgwick convergence time from identity under pure gravity is ~1 s for β = 0.1 (rule of thumb β ≈ convergence rate / sqrt(3)). β = 0.08 gives a small margin against accel noise. Tuning is tracked in `app_config`.

The test host uses its native FPU; the ESP32 uses its single-precision FPU. Both paths execute the same float code. Avoid `double` — a stray promotion triples the cost on-target.

## Verification

Native Unity tests (`test/test_srv_fusion/test_main.cpp`), runs under `env:native` with PlatformIO Unity:

- `test_init_returns_ok_default_beta` — `srv_fusion_init(0.08f)` returns `AG_OK`.
- `test_init_rejects_bad_beta` — `srv_fusion_init(-0.1f)` returns `AG_ERR_ARG`; same for `beta = 2.0f`.
- `test_identity_convergence_under_gravity` — init with β = 0.08; feed `imu_sample_t{ax=0, ay=0, az=9.81, gx=gy=gz=0, t_us += 10000}` for 200 iterations (2 s simulated). Use `TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, out.q0);`.
- `test_quaternion_norm_stable_under_noise` — init; feed 10 000 samples with Gaussian accel + gyro noise (`std = 0.1`). After each call, compute `n = sqrtf(q0^2+q1^2+q2^2+q3^2);` `TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, n);`.
- `test_freefall_frame_does_not_crash` — feed `a = (0,0,0)` for 10 samples; assert `AG_OK` returned and norm still finite.
- `test_null_args_rejected` — `srv_fusion_update(nullptr, &q)` and `srv_fusion_update(&s, nullptr)` each return `AG_ERR_ARG`.
- `test_update_runs_under_200us_host` — surround a single update with `clock_gettime(CLOCK_MONOTONIC)` calls; assert the delta is < 200 000 ns. (Informational; fails soft on slow CI hardware.)

Manual bring-up probes (on-target, one-shot):

- Log `printf("q=[%.3f %.3f %.3f %.3f] dt=%.4f\n", q.q0, q.q1, q.q2, q.q3, dt);` from `t_fusion` once every 100 updates. Expect q drifting smoothly, dt = 0.0100 ± 0.0005 s.

PlatformIO command:

```
pio test -e native -f test_srv_fusion
```

## Rollback / risk

- Smallest revert: replace the body of `srv_fusion_update()` with `*out = (quat_t){1,0,0,0}; return AG_OK;`. Downstream `srv_motion` still runs (produces zero dx/dy because quaternion delta is zero), so the firmware boots and pairs but the cursor does not move.
- Known risks:
  - β too high → orientation wobbles on hand vibration. Early warning: fusion test `test_quaternion_norm_stable_under_noise` norm drifts toward 0.97-0.98 while gyro is nominally still.
  - β too low → orientation lags hand motion. Early warning: 2 s convergence test fails.
  - Accidental `double` literals (`0.5` vs `0.5f`) silently triple on-target runtime. Enforce `-Wdouble-promotion` in the lib's `build_flags` once the scaffold exists.

## References

- architecture.md §4.1 (task `t_fusion`), §5 (timing budget)
- decisions.md ADR-002 (Madgwick, 100 Hz, quaternion)
- epic E06 (scope, acceptance criteria)
- testing-strategy.md §3 (native unit test layer)
- Madgwick, S., "An efficient orientation filter for inertial and inertial/magnetic sensor arrays," 2010 (the canonical IMU-only variant)
