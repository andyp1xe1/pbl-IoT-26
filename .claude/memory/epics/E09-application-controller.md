# E09 — Application Controller (app_controller)

- **Status:** In Progress (implementation complete; on-target + HIL acceptance pending hardware bring-up)
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

- [~] Device boots, pairs with Windows/Linux/macOS host, shows "AirGlove" in the BT devices list (TC-FR01, TC-NFR-HID-001). *Boot chain: `app_controller_start` calls `dd_mpu6050_init` → `dd_touch_init` → `dd_ble_hid_init("AirGlove")` with fail-fast `esp_restart` on any non-AG_OK. HIL verification pending per `docs/plans/09-integration-and-bringup.md`.*
- [~] After a cold reboot with a previously-paired host, reconnection completes ≤ 5 s (TC-FR02). *NimBLE persists bonding automatically (E05); `t_ble_hid_fn` drops into `APP_STATE_PAIRING` on disconnect and re-advertises. HIL test pending.*
- [~] Tilting the glove produces cursor motion in the expected axis; no drift when held still (TC-FR03). *End-to-end pipeline wired: `t_imu_sample` → `q_imu` → `t_fusion` → `q_orientation` → `t_motion` → `q_hid` → `t_ble_hid` → `dd_ble_hid_send`. Axis sign/mapping tuning is reserved for bring-up per plan 06.*
- [~] Thumb↔index pinch produces a single left-click on host; thumb↔middle produces right-click (TC-FR04). *`t_app_fn` maps INDEX press→bit 0, MIDDLE press→bit 1, THUMB ignored (common electrode), RING ignored (Phase II). Button byte is held in `std::atomic<uint8_t> g_current_buttons`, read by `t_motion_fn` so coalesced motion reports carry the live button state.*
- [~] End-to-end p95 latency ≤ 100 ms measured by `docs/srs/testing-strategy.md` procedure (TC-NFR-LAT-001). *`t_ble_hid_fn` throttles at 8 ms (≤ 125 Hz) and coalesces up to 3 extra reports per notify; per-stage budget in `architecture.md` §5 shows ~20 ms typical, ~40 ms p95 in-device. Latency bench pending.*
- [~] No FreeRTOS task stack high-water-mark exceeds 75 % of allocated after 10-minute stress session. *1 Hz heartbeat via FreeRTOS software timer calls `uxTaskGetStackHighWaterMark` on all six task handles and logs over serial; acceptance checked during the 10-min soak in plan 09.*
- [x] `src/main.cpp` contains zero Arduino, ESP-IDF, or lib header includes other than `<stdio.h>` and `app_controller.h`. *Verified: `grep -nE "^#include" src/main.cpp` returns only `<stdio.h>` and `"app_controller.h"`. Public header `app_controller.h` itself exposes only `ag_types.h` — no FreeRTOS / Arduino / NimBLE types leak through; compiled stand-alone in C11 and C++17 without warnings on real content.*

## Dependencies

- E03, E04, E05, E06, E07, E08.

## Progress log

- 2026-04-21: Epic created. Plan: `docs/plans/08-app-controller.md`. End-to-end bring-up checklist: `docs/plans/09-integration-and-bringup.md`.
- 2026-04-23: Implementation delivered. New / updated files:
  - `air-glove/lib/app_controller/library.json` — esp32dev-only manifest; declares every dd_* and srv_* as a dependency.
  - `air-glove/lib/app_controller/include/app_controller.h` — `#pragma once` + enhanced contract doc; exposes only `ag_types.h`; zero FreeRTOS / Arduino surface.
  - `air-glove/lib/app_controller/src/app_controller.cpp` — entry function. Stages: driver init (fail-fast via `esp_restart` after a 5 s log delay) → service init → queue alloc (`xQueueCreate` for q_imu/q_orientation/q_buttons/q_hid with sizes 4/2/8/8) → pinned-core task creation (stacks 2 KB / 4 KB / 2 KB / 4 KB / 3 KB / 4 KB; priorities 5 / 4 / 3 / 3 / 3 / 6; cores 0 / 0 / 0 / 1 / 1 / 1 per `architecture.md` §4) → 1 Hz heartbeat timer logging `uxTaskGetStackHighWaterMark` + FSM state.
  - `air-glove/lib/app_controller/src/tasks.h` — internal header (NOT in `include/`). Declares shared queue handles, the `oriented_frame_t` POD for q_orientation, the six task entry points, and two `std::atomic`s: `g_current_buttons` (shared button byte) and `g_fsm_state` (observable FSM state).
  - `air-glove/lib/app_controller/src/tasks.cpp` — the six task bodies. `queue_put_drop_oldest` helper implements the manual dequeue-then-enqueue drop-oldest policy (xQueueOverwrite doesn't fit depth > 1). `t_app_fn` updates `g_current_buttons` atomically and publishes a button-change report. `t_motion_fn` stamps each motion report with the current button byte so `t_ble_hid_fn`'s last-write-wins coalesce preserves click state (deviation from the plan, which specified buttons=0 on motion reports — would have clobbered press state during coalesce). `t_ble_hid_fn` owns the FSM: `APP_STATE_PAIRING` while disconnected (drains q_hid so producers don't stall); `APP_STATE_ACTIVE` with an 8 ms `xQueueReceive` timeout pacing notifies at ≤ 125 Hz; coalesces up to 3 extra reports (sum dx/dy/wheel saturating to int8, latest buttons).
  - `air-glove/test/test_app_controller/test_main.cpp` — on-target smoke test (4 cases): start returns AG_OK, all six named tasks exist, all four queues are non-NULL, scheduler survives for 1.5 s without task-watchdog reset.
  - `air-glove/platformio.ini` — `env:native` switched from `test_ignore = test_dd_*` to `test_filter = test_srv_*` (whitelist service tests only; automatically excludes the new `test_app_controller` + every future non-service suite).
- 2026-04-23: Local verification. `src/main.cpp` grep confirms only `<stdio.h>` and `"app_controller.h"` includes (NFR-MOD-001). `app_controller.h` compiles stand-alone in C11 and C++17; a simulated stdio-only consumer (`#include <stdio.h> + "app_controller.h"` calling `app_controller_start`) builds with `-Wall -Wextra -Werror`. Regression on the three service libs (srv_fusion / srv_motion / srv_input) still passes `-Werror` native compile. The full FreeRTOS-linked build cannot be run locally (no PlatformIO / ESP-IDF toolchain on host) — `pio run -e esp32dev` plus the on-target smoke test plus the HIL runbook (`docs/plans/09-integration-and-bringup.md`) are the remaining gates before flipping Done.
