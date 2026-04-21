# Plan 08 — app_controller (task wiring + FSM)

- **Epic:** [E09](../../.claude/memory/epics/E09-application-controller.md)
- **Goal:** Provide the single entry point `app_controller_start()` that initialises all `dd_*` drivers, creates the four FreeRTOS queues and six tasks per `architecture.md §4`, runs the top-level FSM (`INIT → PAIRING → ACTIVE`), and merges motion + button events into `hid_mouse_report_t` frames for `dd_ble_hid`.
- **Preconditions:** Plans 02 (dd_mpu6050), 03 (dd_touch), 04 (dd_ble_hid), 05 (srv_fusion), 06 (srv_motion), 07 (srv_input) are implemented and tested.

## Files to create

| Path (under `air-glove/lib/app_controller/`) | Role |
|---|---|
| `include/app_controller.h` | Public header — one function. |
| `src/app_controller.cpp` | Task creation, queue wiring, FSM, heartbeat. |
| `src/tasks.cpp` | Task body functions (`t_imu_sample_fn`, `t_fusion_fn`, etc.). |
| `src/tasks.h` | Internal header — task entry-point prototypes and queue handles. |
| `library.json` | Lib manifest; `env:esp32dev` only (uses FreeRTOS + dd_*). |
| (also update) `air-glove/src/main.cpp` | stdio-only glue — see bottom of this plan. |

## Step-by-step

1. In `tasks.h`, declare extern queue handles:
   - `extern QueueHandle_t q_imu;      // imu_sample_t, depth 4`
   - `extern QueueHandle_t q_orientation; // struct { quat_t q; uint64_t t_us; }, depth 2`
   - `extern QueueHandle_t q_buttons;  // input_event_t, depth 8`
   - `extern QueueHandle_t q_hid;      // hid_mouse_report_t, depth 8`
   - Plus task-entry prototypes: `void t_imu_sample_fn(void*); void t_fusion_fn(void*); void t_touch_fn(void*); void t_motion_fn(void*); void t_app_fn(void*); void t_ble_hid_fn(void*);`
2. In `app_controller.cpp`, implement `app_controller_start()`:
   1. Init drivers in this order (fail-fast on each): `dd_mpu6050_init()` → `dd_touch_init()` → `dd_ble_hid_init("AirGlove")`. On any non-`AG_OK`, `printf("FATAL: init stage X failed code=%d\n", rc); vTaskDelay(pdMS_TO_TICKS(5000)); esp_restart();`.
   2. Init services: `srv_fusion_init(0.08f); srv_motion_init(&default_motion_cfg); srv_input_init(15);`.
   3. Create queues (sizes from glossary + architecture.md §4.2):
      - `q_imu = xQueueCreate(4, sizeof(imu_sample_t));`
      - `q_orientation = xQueueCreate(2, sizeof(oriented_frame_t));` (POD `{quat_t q; uint64_t t_us;}`)
      - `q_buttons = xQueueCreate(8, sizeof(input_event_t));`
      - `q_hid = xQueueCreate(8, sizeof(hid_mouse_report_t));`
      - If any returns `NULL`: `printf("FATAL: queue alloc\n"); esp_restart();`.
   4. Create tasks with `xTaskCreatePinnedToCore(fn, name, stack_words, param, prio, handle, core)`. Stack in WORDS — divide KB by 4. Pass `NULL` for param and store handles in a file-scope array for the heartbeat.
      - `t_imu_sample`: 2 KB / prio 5 / core 0
      - `t_fusion`:     4 KB / prio 4 / core 0
      - `t_touch`:      2 KB / prio 3 / core 0
      - `t_motion`:     4 KB / prio 3 / core 1
      - `t_app`:        3 KB / prio 3 / core 1
      - `t_ble_hid`:    4 KB / prio 6 / core 1
   5. Start the heartbeat timer (see step 6). Return `AG_OK`.
3. Task bodies (`tasks.cpp`):
   - `t_imu_sample_fn`: every 10 ms, `dd_mpu6050_read(&sample); xQueueSend(q_imu, &sample, 0);` Use `vTaskDelayUntil` with a `TickType_t last_wake` seeded from `xTaskGetTickCount()`. On `AG_ERR_IO`, `printf` a ratelimited warning and continue.
   - `t_fusion_fn`: `xQueueReceive(q_imu, &sample, portMAX_DELAY);` then `srv_fusion_update(&sample, &frame.q); frame.t_us = sample.t_us; xQueueOverwrite`... no: `q_orientation` has depth 2 and drop-oldest policy. Use pattern `if (xQueueSend(q_orientation, &frame, 0) != pdTRUE) { xQueueReceive(q_orientation, &trash, 0); xQueueSend(q_orientation, &frame, 0); }`.
   - `t_touch_fn`: `vTaskDelayUntil` 10 ms. `dd_touch_read(&sample); srv_input_process(&sample, evts, 4, &n); for (i=0; i<n; ++i) xQueueSend(q_buttons, &evts[i], 0);` — if send fails (queue full) drop oldest via same dequeue-then-enqueue trick.
   - `t_motion_fn`: on each `q_orientation` arrival, compute `dt = (frame.t_us - last_t_us) * 1e-6f;` then `srv_motion_update(&frame.q, dt, &dx, &dy);`. Build `hid_mouse_report_t{ .dx = dx, .dy = dy, .buttons = 0, .wheel = 0 };` and push into `q_hid`.
   - `t_app_fn`: maintain `static uint8_t s_buttons = 0;` draining `q_buttons`: `TOUCH_PAD_INDEX + PRESS` → `s_buttons |= 0x01` (left); `TOUCH_PAD_INDEX + RELEASE` → `s_buttons &= ~0x01`. Same for `TOUCH_PAD_MIDDLE` and `0x02` (right). Thumb (`TOUCH_PAD_THUMB`) events are IGNORED (thumb is common electrode). Push a button-only `hid_mouse_report_t{ .dx=0, .dy=0, .buttons=s_buttons, .wheel=0 }` whenever `s_buttons` changes.
   - `t_ble_hid_fn`: also owns the top-level FSM. State variable `static app_state_t s_state = INIT;`. Start in `PAIRING`. Loop:
     1. If `!dd_ble_hid_is_connected()` → `s_state = PAIRING;` drain `q_hid` quickly (just consume, don't send) so the producer isn't blocked. Periodically check connect. Once connected → `s_state = ACTIVE;`.
     2. In `ACTIVE`: `xQueueReceive(q_hid, &report, pdMS_TO_TICKS(8));` — 8 ms ≈ 125 Hz throttle. Coalesce: before sending, drain additional reports up to 4 with `xQueueReceive(..., 0)` and sum `dx`/`dy` (saturate to int8), take the latest `buttons`. Then `dd_ble_hid_send(&merged);`. On disconnect during send → back to `PAIRING`.
   - All tasks run `vTaskDelete(NULL)` unreachably (infinite loops).
4. Queue overflow policy per architecture.md §4.2 — drop-oldest:
   - For `q_imu` and `q_orientation`, producers use the dequeue-then-enqueue trick (`xQueueOverwrite` only works on depth-1 queues, so it cannot be used for `q_imu` / `q_orientation` directly — the architecture note in the brief is approximate; rely on the manual pattern).
   - For `q_hid`, coalescing in `t_ble_hid` already drains the queue each interval, so overflow is rare.
5. FSM transitions, concretely:
   - Entry: `INIT` → after `app_controller_start()` returns, we are effectively in `PAIRING`.
   - `PAIRING → ACTIVE`: `dd_ble_hid_is_connected()` flips true.
   - `ACTIVE → PAIRING`: `dd_ble_hid_is_connected()` flips false (host disconnect, out-of-range).
   - No path back to `INIT` — a hard failure triggers `esp_restart()`.
6. Heartbeat (step 2.5): create a FreeRTOS software timer with 1 s period. Callback iterates the stored task handles and prints `printf("hwm %-12s %4u words\n", name, (unsigned) uxTaskGetStackHighWaterMark(handle));` for each. Also log current `s_state` from `t_ble_hid_fn` (atomic read of a static int).
7. Main.cpp (update, do NOT rewrite anything else there):

```cpp
#include <stdio.h>
#include "app_controller.h"

extern "C" void setup() {
  app_controller_start();
}

extern "C" void loop() {
  // empty; all work is in FreeRTOS tasks
}
```

Enforce NFR-MOD-001: the only includes in `src/main.cpp` are `<stdio.h>` and `"app_controller.h"`.

## Public header (target signature)

```c
// lib/app_controller/include/app_controller.h
#pragma once
#include "ag_types.h"

// Start the AirGlove firmware: init drivers, create queues + tasks,
// enter the INIT -> PAIRING -> ACTIVE FSM. Returns AG_OK once tasks
// are running; on fatal init error it does not return (reboots after
// logging). Thread-safety: call exactly once, from setup().
ag_result_t app_controller_start(void);
```

## Internal design notes

- Task priorities follow `architecture.md §4.3`: `t_ble_hid` (6) > `t_imu_sample` (5) > `t_fusion` (4) > `t_touch` / `t_motion` / `t_app` (3). Do not hand-tune further without re-running the NFR-LAT-001 latency test.
- Core pinning: sensor ingest + fusion on core 0; motion mapping + app FSM + BLE on core 1. The only cross-core queue hops are `q_orientation` (core 0 → core 1) and `q_buttons` (core 0 → core 1).
- Shared POD on `q_orientation`: define once in `tasks.h` as `typedef struct { quat_t q; uint64_t t_us; } oriented_frame_t;` — not a new shared type, local to the controller.
- `t_app` and `t_motion` both feed `q_hid`. The consumer `t_ble_hid_fn` merges by summing dx/dy across drained reports and taking the latest button byte. This is the "coalesce" step noted in architecture.md §6 data-flow.
- No mutexes anywhere. Every piece of state is either (a) task-local, (b) behind a queue, or (c) an `std::atomic`-equivalent bool / uint8 (single-word, cores guarantee atomicity on aligned words on ESP32).
- Heartbeat at 1 Hz is cheap and immediately reveals stack bloat during long soak. Tune stack sizes down after soak per architecture.md §4.1.

## Verification

Native tests: **not applicable** — this library includes `freertos/FreeRTOS.h` and the ESP-IDF, which are not available in `env:native`. The layer isolation rule (ADR-005) means `app_controller` is specifically the policy layer that CAN use FreeRTOS primitives; pure-logic pieces (button mapping etc.) were already covered by `srv_input` native tests.

On-target smoke test (`test/test_app_controller/test_main.cpp`, runs under `env:esp32dev`):

- `test_start_returns_ok` — `app_controller_start()` completes without reboot within 3 s.
- `test_tasks_created` — after `start()`, assert all six task handles are non-NULL via `xTaskGetHandle("t_imu_sample")` etc.
- `test_queues_created` — assert `uxQueueMessagesWaiting(q_imu) >= 0` (i.e., handle is valid).
- `test_heartbeat_prints_within_1500ms` — capture serial output; expect one line containing `hwm t_imu_sample` within 1.5 s.

Soak test (manual, 10 minutes on real hardware):

- Start firmware, connect a host, move glove and click repeatedly.
- Capture serial. At t=10 min, assert no task watchdog reset occurred, no `xQueueSend` overflow warnings spammed, and every task's reported high-water stays below 75 % of allocated stack.

Manual bring-up probes:

- `printf("state=%d connected=%d", s_state, dd_ble_hid_is_connected());` every 1 s — verifies FSM transitions during pairing.
- Break on `t_ble_hid_fn` and inspect the merged report; move glove and watch `dx`/`dy` evolve.

PlatformIO command:

```
pio test -e esp32dev -f test_app_controller
```

## Rollback / risk

- Smallest revert: replace `app_controller_start()` with `printf("boot\n"); return AG_OK;`. Firmware boots, no tasks, no BLE — degrades cleanly.
- Known risks:
  - Queue drop-oldest via dequeue-then-enqueue is not atomic; a higher-priority producer between the two calls can lose an item. Mitigation: this pattern runs from within the producer task only, so it is effectively atomic on a single core, and cross-core producers of the same queue do not exist.
  - Stack sizes are estimates; under Madgwick's `sqrtf` expansion or with `printf` in a tight loop they can blow up. Mitigation: heartbeat high-water report + 75 % ceiling in soak test.
  - BLE disconnect during a send can leak the current `report` — not a correctness issue, just a lost frame. Acceptable.
  - `t_app` only publishes on button CHANGE; if a button event is missed the state is stuck. Mitigation: `srv_input` guarantees PRESS/RELEASE pair per edge; any missed event indicates a bug upstream.

## References

- architecture.md §4 (task table), §4.3 (priority rationale), §6 (data flow), §7 (error handling), §8 (cross-core)
- decisions.md ADR-005 (stdio-only main), ADR-006 (FreeRTOS + pinned tasks + queues), ADR-007 (Phase I scope)
- epic E09 (public interface, acceptance criteria)
- glossary.md (task table + queue table)
- testing-strategy.md §2 (FR-001–FR-004 covered by end-to-end HIL after controller lands)
