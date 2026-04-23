# E07 — Motion Mapping Service (srv_motion)

- **Status:** Done
- **Phase:** I
- **Owns:** `air-glove/lib/srv_motion/`
- **Plan:** `docs/plans/06-srv-motion.md`
- **Realises:** FR-003, NFR-STAB-001, NFR-LAT-001

## Goal

Transform the quaternion stream from `srv_fusion` into smooth, capped `hid_mouse_report_t`-compatible `dx, dy` integers. Rate-control mapping (glove tilt magnitude → cursor speed) with dead-zone, non-linear gain curve, and a clutch gate. Pure C — unit-testable on host.

## Scope

**In:**
- Extract yaw/pitch rates (or cursor-plane tilt) from quaternion deltas.
- Dead-zone: inputs below a configurable magnitude produce zero output (kills idle-hand tremor).
- Non-linear gain: small tilts → fine cursor, large tilts → fast travel; monotonic and continuous.
- Velocity cap: |dx|, |dy| ≤ 127 (int8 range).
- Clutch gate: when active, output is forced to (0, 0) regardless of input.
- Native unit tests in `test/test_srv_motion/`: dead-zone, gain monotonicity, sign preservation, clutch zeroing.

**Out:**
- The clutch trigger condition — that's `app_controller` (E09) reading `srv_input`.
- Scroll mapping — E10 (backlog).

## Public interface

```c
// srv_motion/include/srv_motion.h
#include "ag_types.h"
#include "srv_fusion.h"

typedef struct {
  float deadzone_rad;     // below this |tilt| per sample, output is zero
  float gain_low;         // slope of linear region
  float gain_exp;         // exponent of non-linear segment (> 1 expands fast moves)
  float velocity_cap;     // absolute cap before clamping to int8
} motion_config_t;

ag_result_t srv_motion_init(const motion_config_t *cfg);
ag_result_t srv_motion_update(const quat_t *q, float dt_s, int8_t *dx, int8_t *dy);
void        srv_motion_set_clutch(bool active);
void        srv_motion_reset(void);
```

## Acceptance criteria

- [x] Native test: zero quaternion delta produces `dx == 0 && dy == 0`. *Covered by `test_identical_q_returns_zero`; 0 passes.*
- [x] Native test: input below `deadzone_rad` produces zero output. *Covered by `test_below_deadzone_returns_zero`; passes at θ = deadzone/2.*
- [x] Native test: doubling tilt input never decreases output magnitude (monotonic). *Covered by `test_doubling_input_is_monotonic` over θ ∈ [0.03, 0.30] rad; passes.*
- [x] Native test: `srv_motion_set_clutch(true)` zeros output until released. *Covered by `test_clutch_zeros_output`; engaged → 0, released + fresh delta → non-zero.*
- [x] Native test: output sign matches input-tilt sign in each axis. *Covered by `test_sign_preservation` (positive/negative pitch → dx sign; positive/negative roll → dy sign).*
- [x] Output is always in `[-127, +127]`. *Covered by `test_output_bounded_to_int8` with θ = ±1.5 rad (far above cap); passes.*

## Dependencies

- E02, E06.

## Progress log

- 2026-04-21: Epic created. Rate-control choice per research SRS `ch3_proposed_solution.tex`. Plan: `docs/plans/06-srv-motion.md`.
- 2026-04-23: Implementation delivered. New files:
  - `air-glove/lib/srv_motion/library.json` — PlatformIO manifest; depends on `app_config` + `srv_fusion`.
  - `air-glove/lib/srv_motion/include/srv_motion.h` — full contract header (units, invariants, thread-safety per field/function), `#pragma once` for consistency with `srv_fusion`.
  - `air-glove/lib/srv_motion/src/srv_motion.cpp` — full implementation (~135 lines). Algorithm per plan 06: q_delta via unit-quaternion conjugate, small-angle rotation vector (2 × delta.vec), per-axis dead-zone, non-linear gain (`gain_low·|θ| + |θ|^gain_exp`, sign via `copysignf`), velocity cap, int8 clamp. State: `s_cfg`, `s_q_prev`, `s_has_prev`, `s_clutch` (~32 bytes).
  - `air-glove/test/test_srv_motion/test_main.cpp` — 11 native Unity tests replacing the placeholder.
- 2026-04-23: Verification complete. `g++ -std=gnu++17 -Wall -Wextra -Werror` compile of `srv_motion.cpp` passes against native include paths. `nm` confirms only the four public `extern "C"` symbols are exported; state + helpers stay file-scope static. A mini-Unity shim on the host executed all 11 tests — **11/11 pass, 0 failures**. All six acceptance criteria are covered by a passing test. Status: Done.
