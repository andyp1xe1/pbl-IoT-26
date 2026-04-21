# Plan 06 — srv_motion (quaternion to dx/dy mapping)

- **Epic:** [E07](../../.claude/memory/epics/E07-motion-mapping-service.md)
- **Goal:** Convert the quaternion stream from `srv_fusion` into `int8_t dx, dy` cursor deltas with dead-zone, non-linear gain, velocity cap, and a clutch gate. Pure C; builds and tests under `env:native`.
- **Preconditions:** E06 (`srv_fusion.h` defines `quat_t`). E02 defines `ag_result_t` and `AG_OK`/`AG_ERR_ARG`. Plan 01 has scaffolded `air-glove/lib/srv_motion/`.

## Files to create

| Path (under `air-glove/lib/srv_motion/`) | Role |
|---|---|
| `include/srv_motion.h` | Public header (config struct + init/update/clutch/reset). |
| `src/srv_motion.cpp` | Pure C++ implementation. |
| `library.json` | Lib manifest, compatible with `native` and `esp32dev`. |
| `test/test_srv_motion/test_main.cpp` | Native Unity tests. |

## Step-by-step

1. Declare the `motion_config_t` struct and four functions in `srv_motion.h` verbatim from the epic.
2. In `srv_motion.cpp`, include `<math.h>`, `<stdint.h>`, `<stdbool.h>` only. File-scope state:
   - `static motion_config_t s_cfg = { .deadzone_rad = 0.02f, .gain_low = 400.0f, .gain_exp = 1.6f, .velocity_cap = 127.0f };`
   - `static quat_t s_q_prev = { 1.0f, 0.0f, 0.0f, 0.0f };`
   - `static bool s_has_prev = false;`
   - `static bool s_clutch = false;`
3. Implement `srv_motion_init(const motion_config_t *cfg)`:
   - If `cfg == nullptr` → `AG_ERR_ARG`.
   - Copy into `s_cfg`. Reject non-finite or negative fields (`deadzone_rad < 0`, `velocity_cap <= 0`, `gain_low <= 0`, `gain_exp < 1.0f`) → `AG_ERR_ARG`.
   - `s_has_prev = false; s_clutch = false;`
   - Return `AG_OK`.
4. Implement `srv_motion_set_clutch(bool active)` → simply sets `s_clutch = active;`. No return value.
5. Implement `srv_motion_reset(void)` → `s_has_prev = false; s_clutch = false;`.
6. Implement `srv_motion_update(const quat_t *q, float dt_s, int8_t *dx, int8_t *dy)`:
   1. Null-check `q`, `dx`, `dy` → `AG_ERR_ARG`.
   2. On first call (`!s_has_prev`), cache `s_q_prev = *q; s_has_prev = true; *dx = 0; *dy = 0;` return `AG_OK`.
   3. If `s_clutch`, write `*dx = 0; *dy = 0;` and still update `s_q_prev = *q`; return `AG_OK`. (Updating prev prevents a jump when clutch releases.)
   4. Compute `q_delta = q_prev_inverse * q_curr`. Conjugate of unit quaternion: `(q0, -q1, -q2, -q3)`. Multiply: a standard 16-mul quaternion product.
   5. Small-angle approximation: the vector part of `q_delta` equals `0.5 * angle * axis` (for small angles). Take `dtheta_x = 2.0f * q_delta.q1;`, `dtheta_y = 2.0f * q_delta.q2;`. (We use roll-axis rotation → dy-like motion, and pitch-axis → dx-like motion; final axis assignment is tuned during bring-up.)
   6. Axis mapping (Phase I): `theta_pitch = dtheta_y` maps to `dx`, `theta_roll = dtheta_x` maps to `dy`. Sign may need flipping at bring-up — document the observed sign in the bring-up log.
   7. Convert to rate: `rx = theta_pitch / dt_s; ry = theta_roll / dt_s;` (rad/s). Guard `dt_s < 1e-4f` → return cached zeros.
   8. Dead-zone (per axis on the rate magnitude): `if (fabsf(rx) < s_cfg.deadzone_rad / dt_s) rx = 0.0f;` — or equivalently on angular magnitude. Pick the angular variant for readability: `if (fabsf(dtheta_pitch) < s_cfg.deadzone_rad) dtheta_pitch = 0;` and propagate.
   9. Non-linear gain: `out_x = copysignf(s_cfg.gain_low * fabsf(theta_pitch) + powf(fabsf(theta_pitch), s_cfg.gain_exp), theta_pitch);` (ensures sign preserved, monotonic in `|theta|`, continuous at zero).
   10. Cap at `velocity_cap`: `if (fabsf(out_x) > s_cfg.velocity_cap) out_x = copysignf(s_cfg.velocity_cap, out_x);`
   11. Clamp to int8: `*dx = (int8_t) lrintf(out_x);` same for dy. A final `if > 127` / `< -127` guard.
   12. `s_q_prev = *q;`
   13. Return `AG_OK`.
7. Justification for quaternion-delta over "forward-vector projection": both work, but the delta approach is frame-invariant (does not hard-code which world axis is "up") and more straightforward to verify in unit tests (`q_prev == q_curr` → zero delta by construction).

## Public header (target signature)

```c
// lib/srv_motion/include/srv_motion.h
#pragma once
#include "ag_types.h"
#include "srv_fusion.h"   // for quat_t

typedef struct {
  float deadzone_rad;    // below this |tilt| per sample, output is zero
  float gain_low;        // slope of linear region (int8 units per rad of tilt)
  float gain_exp;        // exponent of non-linear segment (>= 1)
  float velocity_cap;    // absolute cap before clamp to int8 (<= 127)
} motion_config_t;

ag_result_t srv_motion_init(const motion_config_t *cfg);
ag_result_t srv_motion_update(const quat_t *q, float dt_s, int8_t *dx, int8_t *dy);
void        srv_motion_set_clutch(bool active);
void        srv_motion_reset(void);
```

## Internal design notes

- Inversion of a unit quaternion is a conjugate, not a true inverse — much cheaper and exact for unit norm.
- Small-angle twice-the-imag extraction: for any unit quaternion `(cos(θ/2), sin(θ/2)·n̂)` with small θ, `sin(θ/2)·n̂ ≈ (θ/2)·n̂`, so `2·q_vec ≈ θ·n̂`. The x/y/z components are the rotation-vector components.
- The epic explicitly says "dead-zone on the per-axis angular-rate magnitude". Step 8 above applies it in the angular-delta domain because `dt_s` is a constant 10 ms in practice; the two formulations are equivalent up to a scale factor, and applying it in angular space avoids a spurious near-singular divide when `dt_s` drifts.
- Gain curve `g(|θ|) = k_low·|θ| + |θ|^exp` — strictly increasing for `|θ| > 0` when both coefficients are positive and `exp > 1`, so the monotonicity test passes by construction. `copysignf` restores the sign.
- No heap; total state is one quaternion plus a config struct plus two bools (~40 bytes).
- Thread-safety: called only from `t_motion` (single-core-pinned). No mutex needed.

## Verification

Native Unity tests (`test/test_srv_motion/test_main.cpp`), runs under `env:native`:

- `test_init_rejects_null` — `srv_motion_init(nullptr)` returns `AG_ERR_ARG`.
- `test_init_rejects_bad_config` — negative `deadzone_rad`, `gain_low <= 0`, `gain_exp < 1.0f`, `velocity_cap <= 0` → `AG_ERR_ARG`.
- `test_first_call_returns_zero` — after init, first `srv_motion_update` with any quaternion yields `dx == 0 && dy == 0`.
- `test_identical_q_returns_zero` — feed the same quaternion twice; second call yields `dx == 0 && dy == 0`.
- `test_below_deadzone_returns_zero` — feed a `q_delta` that corresponds to |dtheta| < deadzone_rad; assert zero output.
- `test_doubling_input_is_monotonic` — for a ramp of `|dtheta|` from `2·deadzone` to the cap, assert `|dx(k+1)| >= |dx(k)|` across the ramp.
- `test_sign_preservation` — positive pitch → positive `dx`; negative pitch → negative `dx`. Same for roll → dy.
- `test_clutch_zeros_output` — set clutch true, feed non-zero delta → `dx == 0 && dy == 0`. Clear clutch → output returns.
- `test_output_bounded_to_int8` — feed an extreme `q_delta` → `|dx| <= 127 && |dy| <= 127`.
- `test_reset_reestablishes_first_frame` — after `srv_motion_reset()`, next update yields zero output (no jump).

PlatformIO command:

```
pio test -e native -f test_srv_motion
```

## Rollback / risk

- Smallest revert: stub `srv_motion_update()` to `*dx = 0; *dy = 0; return AG_OK;`. Firmware runs; cursor never moves.
- Known risks:
  - Axis mapping (pitch↔dx vs roll↔dx) and sign are user-perception-dependent; the test suite cannot detect a swap. Mitigation: the TC-FR03-01 HIL test (tilt right → cursor right) explicitly checks this.
  - At very small `dt_s` (< 1 ms) the rate explodes. Mitigation: guard in step 7 + the fusion layer already clamps `dt` to `[1, 50]` ms upstream.
  - `powf(x, exp)` on ESP32 is implemented in software; measured cost ~10-15 µs. Acceptable at 100 Hz; flag if `t_motion` overruns its budget.

## References

- architecture.md §4.1 (task `t_motion`), §6.1 (motion path)
- decisions.md (implicit — rate-control mapping justified in `report/chapters/ch3_proposed_solution.tex`)
- epic E07 (scope, public interface, acceptance)
- testing-strategy.md §3 (native unit tests enumerate the exact cases above)
