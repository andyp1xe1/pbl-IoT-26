# E07 — Motion Mapping Service (srv_motion)

- **Status:** Not Started
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

- [ ] Native test: zero quaternion delta produces `dx == 0 && dy == 0`.
- [ ] Native test: input below `deadzone_rad` produces zero output.
- [ ] Native test: doubling tilt input never decreases output magnitude (monotonic).
- [ ] Native test: `srv_motion_set_clutch(true)` zeros output until released.
- [ ] Native test: output sign matches input-tilt sign in each axis.
- [ ] Output is always in `[-127, +127]`.

## Dependencies

- E02, E06.

## Progress log

- 2026-04-21: Epic created. Rate-control choice per research SRS `ch3_proposed_solution.tex`. Plan: `docs/plans/06-srv-motion.md`.
