# E04 — Touch Driver (dd_touch)

- **Status:** In Progress (implementation complete; on-target acceptance pending hardware bring-up)
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

- [~] On-target test: each pad baseline reading stable (< 5 % variance) over 1 s idle. *Test `test_baseline_stable_1s` written (100 samples at 10 ms cadence, relative stddev threshold 0.05). Pending hardware run.*
- [~] Touching a pad drops its raw reading by ≥ 30 % within 50 ms. *Interactive test `test_touch_drops_reading_INDEX` written (3 s detection window, 30 % drop threshold, `touched_mask` bit assertion). Pending hardware run. MIDDLE / RING pads can be exercised by duplicating the test — kept single-pad for Phase I minimalism.*
- [x] All four pads read under a single `dd_touch_read()` call with consistent timestamps. *Driver fills `out->raw[0..3]` then sets `out->touched_mask` and `out->t_us` once — single atomic update verified by code review.*
- [x] Auto-calibration runs at init; manual re-calibration API is NOT required in Phase I. *`dd_touch_init()` performs 50-sample baseline per pad (+ 200 µs spacing) and sets `threshold = baseline * 0.7`. No extra API exposed.*

## Dependencies

- E01, E02.

## Progress log

- 2026-04-21: Epic created. Pin allocation confirmed in `docs/srs/hardware.md`; plan in `docs/plans/03-dd-touch.md`.
- 2026-04-21: Implementation delivered. New / updated files:
  - `air-glove/lib/dd_touch/src/dd_touch.cpp` — full driver. `dd_touch_init()` primes the peripheral, then for each of T0/T2/T3/T4 averages 50 samples (200 µs apart) and sets `threshold[i] = baseline[i] * 0.7`. `dd_touch_read()` reads all four pads via `touchRead()`, assembles `touched_mask`, and updates untouched-pad baselines via an EMA (α = 0.01, ≈ 1 s time constant at 100 Hz).
  - `air-glove/test/test_dd_touch/test_main.cpp` — six on-target Unity tests: init OK, all pads populate, timestamp monotonic, baseline relative stddev < 5 % (acceptance), calibration repeatable within 10 % across two runs, and an interactive drop-on-touch probe for the INDEX pad.
- 2026-04-21: Verification — shim compile of `dd_touch.cpp` with `g++ -Wall -Wextra -Werror` passes. `nm` confirms only the two public `extern "C"` symbols are exported (`dd_touch_init`, `dd_touch_read`); `s_baseline[]`, `s_threshold[]`, `s_initialized`, `kGpio[]`, and `apply_ratio()` stay file-scope static. Hardware bring-up (`pio test -e esp32dev -f test_dd_touch` on a DevKit-C with pads wired per `docs/srs/hardware.md`) is the remaining gate to flip Done.
