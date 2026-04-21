# E09 — Application Controller (app_controller)

- **Status:** Not Started
- **Phase:** I
- **Owns:** `air-glove/lib/app_controller/`, `air-glove/lib/app_config/`
- **Plan:** `docs/plans/08-app-controller.md`, bring-up in `docs/plans/09-integration-and-bringup.md`
- **Realises:** FR-001, FR-002, FR-003, FR-004, NFR-LAT-001 (end-to-end)

## Goal

The single entry point to Phase I firmware. Starts all FreeRTOS tasks per `docs/srs/architecture.md` §4, creates the four inter-task queues, runs the top-level FSM (`INIT → PAIRING → ACTIVE`), and merges motion + input events into `hid_mouse_report_t` frames for `dd_ble_hid`.

## Scope

**In:**
- `app_controller_start()` entry (called from `src/main.cpp`).
- Task creation with pinned cores (core 0 = input/sensor/fusion; core 1 = motion/app/BLE), stack sizes per glossary.
- Queue creation: `q_imu`, `q_orientation`, `q_buttons`, `q_hid`.
- FSM:
  - `INIT` → run `dd_*_init()` in order, fail-fast on any error (heartbeat `printf` logs).
  - `PAIRING` → if `dd_ble_hid_is_connected() == false`, stay here; allow cursor input to be queued but not sent.
  - `ACTIVE` → merge latest motion dx/dy + button state (from `srv_input`) into report, call `dd_ble_hid_send()`. Throttle to ≤ 125 Hz.
- Button mapping: index-pad press → bit 0 (left click), middle-pad press → bit 1 (right click). Thumb pad is the common — never mapped directly.
- Watchdog: on `dd_ble_hid` disconnect, transition back to `PAIRING`.
- `src/main.cpp` contains only `#include <stdio.h>` + `extern int app_controller_start(void);` and the `setup()`/`loop()` glue.

**Out:**
- Scroll, clutch, sensitivity profiles, calibration — Phase II epics E10–E12.
- Deep sleep, anomaly recovery, power monitoring — Phase III epics E13–E14.

## Public interface

```c
// app_controller/include/app_controller.h
ag_result_t app_controller_start(void);
```

## Acceptance criteria

- [ ] Device boots, pairs with Windows/Linux/macOS host, shows "AirGlove" in the BT devices list (TC-FR01, TC-NFR-HID-001).
- [ ] After a cold reboot with a previously-paired host, reconnection completes ≤ 5 s (TC-FR02).
- [ ] Tilting the glove produces cursor motion in the expected axis; no drift when held still (TC-FR03).
- [ ] Thumb↔index pinch produces a single left-click on host; thumb↔middle produces right-click (TC-FR04).
- [ ] End-to-end p95 latency ≤ 100 ms measured by `docs/srs/testing-strategy.md` procedure (TC-NFR-LAT-001).
- [ ] No FreeRTOS task stack high-water-mark exceeds 75 % of allocated after 10-minute stress session.
- [ ] `src/main.cpp` contains zero Arduino, ESP-IDF, or lib header includes other than `<stdio.h>` and `app_controller.h`.

## Dependencies

- E03, E04, E05, E06, E07, E08.

## Progress log

- 2026-04-21: Epic created. Plan: `docs/plans/08-app-controller.md`. End-to-end bring-up checklist: `docs/plans/09-integration-and-bringup.md`.
