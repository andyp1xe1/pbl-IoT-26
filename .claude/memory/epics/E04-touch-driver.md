# E04 — Touch Driver (dd_touch)

- **Status:** Not Started
- **Phase:** I
- **Owns:** `air-glove/lib/dd_touch/`
- **Plan:** `docs/plans/03-dd-touch.md`
- **Realises:** FR-004, partial NFR-ERG-001 (no exposed conductors)

## Goal

Capacitive touch driver for four fingertip pads (thumb T0, index T2, middle T3, ring T4) using the ESP32 native touch peripheral. Provides raw readings and a boolean "touched" mask. Per-pad thresholds are auto-calibrated at init (assume fingers are not in contact) and updated over a running baseline to survive thermal/hand-moisture drift.

## Scope

**In:**
- Init all four touch channels (GPIO4 / GPIO2 / GPIO15 / GPIO13).
- Baseline calibration: sample each pad 50× at init, compute mean → baseline. Threshold = baseline − `(baseline × 0.3)`. Configurable via `app_config` later (Phase II).
- Slow-baseline update: exponential moving average over untouched samples to fight drift.
- `dd_touch_read()` returns raw values + a bitmask of touched pads, all under one timestamp.
- On-target Unity test: all four channels read stable baselines; touching a pad lowers its raw value.

**Out:**
- Debounce / edge-detection / chord logic — that's `srv_input` (E08).
- Sleep-wake-on-touch — Phase III power-management (E14).

## Public interface

See E02 (`dd_touch.h`).

## Acceptance criteria

- [ ] On-target test: each pad baseline reading stable (< 5 % variance) over 1 s idle.
- [ ] Touching a pad drops its raw reading by ≥ 30 % within 50 ms.
- [ ] All four pads read under a single `dd_touch_read()` call with consistent timestamps.
- [ ] Auto-calibration runs at init; manual re-calibration API is NOT required in Phase I (thumb-as-common wiring makes calibration cheap).

## Dependencies

- E01, E02.

## Progress log

- 2026-04-21: Epic created. Pin allocation confirmed in `docs/srs/hardware.md`; plan in `docs/plans/03-dd-touch.md`.
