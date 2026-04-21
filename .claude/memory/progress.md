# Air Glove — Progress Dashboard

Last updated: 2026-04-21 (E01 + E02 closed; E03 implementation landed, on-target test pending)

Legend: `Not Started` · `In Progress` · `Blocked` · `Done` · `Backlog` (not yet promoted to active planning).

## Phase I — MVP (active)

| Epic | Name | Status | % | Owner | Plan | Last update | Notes |
|------|------|--------|---|-------|------|-------------|-------|
| E01 | Project Foundation | Done | 100 | — | `docs/plans/01-scaffolding.md` | 2026-04-21 | Scaffold delivered; `pio run` / `pio test` verification deferred until PlatformIO is installed on the host |
| E02 | HW Abstraction Layer | Done | 100 | — | `docs/plans/01-scaffolding.md` | 2026-04-21 | All four public headers compile stand-alone `-Werror` in C11 + C++17; contract comments added |
| E03 | IMU Driver (dd_mpu6050) | In Progress | 80 | — | `docs/plans/02-dd-mpu6050.md` | 2026-04-21 | Driver + regs + on-target Unity suite landed; shim compile clean; `pio test -e esp32dev -f test_dd_mpu6050` on real hardware is the remaining gate |
| E04 | Touch Driver (dd_touch) | Not Started | 0 | — | `docs/plans/03-dd-touch.md` | 2026-04-21 | T0/T2/T3/T4 channels, thresholds |
| E05 | BLE HID Driver (dd_ble_hid) | Not Started | 0 | — | `docs/plans/04-dd-ble-hid.md` | 2026-04-21 | NimBLE, HID mouse profile, reconnect |
| E06 | Sensor Fusion Service (srv_fusion) | Not Started | 0 | — | `docs/plans/05-srv-fusion.md` | 2026-04-21 | Madgwick @ 100 Hz |
| E07 | Motion Mapping (srv_motion) | Not Started | 0 | — | `docs/plans/06-srv-motion.md` | 2026-04-21 | Rate-control, dead-zone, gain |
| E08 | Input Service (srv_input) | Not Started | 0 | — | `docs/plans/07-srv-input.md` | 2026-04-21 | Debounce, edges, chords |
| E09 | Application Controller (app_controller) | Not Started | 0 | — | `docs/plans/08-app-controller.md` | 2026-04-21 | Tasks, queues, FSM |

## Backlog — Phase II (planned after Phase I demo)

| Epic | Name | Status | Notes |
|------|------|--------|-------|
| E10 | Scroll & Clutch | Backlog | Builds on srv_input + srv_motion |
| E11 | Sensitivity Switch | Backlog | Adds hardware toggle path + profile store |
| E12 | Calibration | Backlog | Persists neutral orientation + bias |

## Backlog — Phase III (robustness)

| Epic | Name | Status | Notes |
|------|------|--------|-------|
| E13 | Fault Safety | Backlog | Anomaly detection, watchdog, recovery |
| E14 | Power Management | Backlog | Deep sleep, wake sources, battery gauge |

## Milestones

| Milestone | Target | Status |
|-----------|--------|--------|
| M0 — Documentation scaffold complete | 2026-04-21 | Done (2026-04-21) — 14 epics + 5 SRS docs + 10 plans written |
| M1 — PlatformIO skeleton builds (both envs) | 2026-04-22 | Partial — `srv_*` libs compile under g++ with -Wall -Wextra; full `pio run -e esp32dev` / `pio run -e native` pending on a host with PlatformIO installed |
| M2 — Phase I MVP bench demo (cursor + click over BLE) | TBD | Not Started |

---

**How to update this file:** when an epic status changes, update its row (status, %, last update, notes). When a milestone is hit, update the milestone table. Never delete rows — mark completed epics `Done` and keep them visible.
