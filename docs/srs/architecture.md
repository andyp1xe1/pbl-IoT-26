# Air Glove — Architecture (Phase I MVP)

## 1. Purpose

This document defines the software architecture for the Air Glove firmware, a wearable BLE-HID mouse built on an ESP32 with an MPU6050 IMU and native capacitive touch finger pads. It specifies the layered component model, FreeRTOS task graph, data flow, and timing budget that together implement the Phase I scope (BLE pairing with auto-reconnect, pointer XY motion, left and right click). It is the contract between the SRS (see `report/chapters/requirements.tex`, `report/chapters/system_design.tex`, `report/chapters/ch3_proposed_solution.tex`) and the PlatformIO source tree under `air-glove/`. Design rationale for each major choice is tracked in `docs/srs/decisions.md`.

## 2. Layered architecture

The firmware is split into three strict layers. Dependencies flow downward only.

```
+----------------------------------------------------------+
|                    app_* (policy)                        |
|  app_controller, app_config                              |
|  Orchestrates tasks, owns queues, merges state.          |
+----------------------------------------------------------+
                           |
                           v
+----------------------------------------------------------+
|                  srv_* (pure domain)                     |
|  srv_fusion, srv_motion, srv_input                       |
|  Math + policy. No Arduino.h, no ESP-IDF, host-testable. |
+----------------------------------------------------------+
                           |
                           v
+----------------------------------------------------------+
|                 dd_* (device drivers)                    |
|  dd_mpu6050, dd_touch, dd_ble_hid                        |
|  Thin hardware wrappers. ONLY layer allowed to include   |
|  Arduino.h / ESP-IDF / NimBLE headers.                   |
+----------------------------------------------------------+
                           |
                           v
                     ESP32 hardware
         (I2C, touch controller, BLE radio, NVS)
```

Rules:

- `app_*` may depend on `srv_*` headers and on `app_config`. It must not include hardware headers directly.
- `srv_*` is pure C++; no platform includes. This enables the PlatformIO `native` environment to unit-test fusion and motion math on the developer host.
- `dd_*` is the only layer allowed to `#include <Arduino.h>`, `#include <Wire.h>`, `#include <NimBLEDevice.h>`, or ESP-IDF drivers. See ADR-005 in `docs/srs/decisions.md`.
- `src/main.cpp` uses `<stdio.h>` only and calls `app_controller_start()`.

## 3. Component map

All libraries live under `air-glove/lib/`.

| Folder | Layer | Responsibility | Epic |
|---|---|---|---|
| `dd_mpu6050` | dd | I2C init (GPIO21 SDA, GPIO22 SCL, 400 kHz), raw accel/gyro read, scaling, self-test hooks. | E03 |
| `dd_touch` | dd | Configure ESP32 touch channels T0/T2/T3/T4, expose raw filtered readings. | E04 |
| `dd_ble_hid` | dd | NimBLE HID mouse profile: advertising, pairing, auto-reconnect, `send_report(dx, dy, buttons)`. | E05 |
| `srv_fusion` | srv | Madgwick filter, 100 Hz update, quaternion output + bias tracking. | E06 |
| `srv_motion` | srv | Quaternion delta to cursor dx/dy mapping, gain/deadzone, optional smoothing. | E07 |
| `srv_input` | srv | Debounce + edge detection on touch pad states, maps pads to L/R click. | E08 |
| `app_controller` | app | Task creation, queue wiring, watchdog, lifecycle (start/stop). | E09 |
| `app_config` | app | Compile-time constants (pins, thresholds, gains, task priorities, stack sizes) and shared POD types. | E02 (shared types live under the HW abstraction epic) |

## 4. Runtime model

### 4.1 Task table

| Task | Period / trigger | Core | Stack (est.) | Priority | Producer -> Consumer |
|---|---|---|---|---|---|
| `t_imu_sample` | 10 ms (100 Hz) | 0 | 2 KB | high (5) | `dd_mpu6050` -> `q_imu` |
| `t_fusion` | blocks on `q_imu` | 0 | 4 KB | med-high (4) | Madgwick -> `q_orientation` |
| `t_touch` | 10 ms | 0 | 2 KB | med (3) | `dd_touch` -> `q_buttons` |
| `t_motion` | blocks on `q_orientation` | 1 | 4 KB | med (3) | dx/dy -> `q_hid` (motion part) |
| `t_app` | blocks on `q_buttons` + motion events | 1 | 3 KB | med (3) | merged state -> `q_hid` |
| `t_ble_hid` | blocks on `q_hid` | 1 | 4 KB | highest (6) | `dd_ble_hid.send_report()` |

Stack sizes are initial estimates; final values come from `uxTaskGetStackHighWaterMark()` during soak tests and may be tuned down.

### 4.2 Queue table

| Queue | Item | Depth | Producer | Consumer | Overflow policy |
|---|---|---|---|---|---|
| `q_imu` | raw IMU sample (int16 ax,ay,az,gx,gy,gz + ts) | 4 | `t_imu_sample` | `t_fusion` | drop-oldest (stale frames are useless) |
| `q_orientation` | quaternion + ts | 2 | `t_fusion` | `t_motion` | drop-oldest |
| `q_buttons` | packed pad state byte + edge flags + ts | 8 | `t_touch` | `t_app` | block shortly, then drop-oldest |
| `q_hid` | HID mouse report (dx,dy,buttons) | 8 | `t_motion`, `t_app` | `t_ble_hid` | drop-oldest, coalesce dx/dy |

### 4.3 Priority rationale

- `t_ble_hid` is highest so HID reports drain promptly once a connection interval window opens; blocking this task adds perceptible lag.
- `t_imu_sample` is next because missing the 10 ms tick corrupts the Madgwick integration step.
- `t_fusion` runs med-high; it cannot be preempted indefinitely or the orientation falls behind real hand motion.
- `t_touch`, `t_motion`, and `t_app` share medium priority; they are all bounded-work consumers of fresh data.
- All tasks sit above the FreeRTOS idle priority; no task runs at the system-reserved top priorities (timer service stays highest system-side).

## 5. Timing budget

End-to-end target: motion at the hand visible as cursor movement within NFR-LAT-001 budget (<= 100 ms). Estimates below are typical values on ESP32 at 240 MHz with WiFi disabled.

| Stage | p50 (est.) | p95 (est.) | Notes |
|---|---|---|---|
| IMU sample acquisition | 1 ms | 2 ms | I2C read at 400 kHz, ~14 bytes. |
| Queue hop `q_imu` | < 1 ms | 1 ms | same-core queue. |
| Madgwick update | < 2 ms | 2 ms | float math, fixed iteration. |
| Queue hop `q_orientation` (cross-core) | < 1 ms | 2 ms | core 0 -> core 1. |
| Motion mapping | < 1 ms | 1 ms | quaternion delta + scaling. |
| Queue hop `q_hid` | < 1 ms | 1 ms | same-core queue. |
| BLE HID notify (connection interval) | ~15 ms | ~30 ms | negotiated 7.5-30 ms interval. |
| Host stack render | platform-dependent | platform-dependent | out of scope. |
| **Total in-device p95 (est.)** | **~20 ms** | **~40 ms** | fits inside 100 ms budget. |

BLE connection interval dominates the worst case; we request a short interval on connection but the host may clamp it.

See `report/chapters/ch3_proposed_solution.tex` for the higher-level timing narrative.

## 6. Data flow

### 6.1 Motion path (one cursor tick)

```
core 0                                        core 1
------                                        ------
 [t_imu_sample]                                [t_motion]
   | read MPU6050 over I2C                       ^
   v                                             |
  q_imu (depth 4) ---------+                     |
   |                       |                     |
   v                       |                     |
 [t_fusion]                |                     |
   | Madgwick update       |                     |
   v                       |                     |
  q_orientation (depth 2) -+--- cross-core ----> |
                                                 |
                                            dx/dy computed
                                                 v
                                            q_hid (depth 8)
                                                 |
                                                 v
                                           [t_ble_hid]
                                                 |
                                                 v
                                       dd_ble_hid.send_report()
                                                 |
                                                 v
                                           BLE radio -> host
```

### 6.2 Touch path (parallel)

```
core 0                                        core 1
------                                        ------
 [t_touch]
   | touchRead T0/T2/T3/T4
   v
  q_buttons (depth 8) ------- cross-core ----> [t_app]
                                                 |
                                         debounce + map to L/R
                                                 v
                                            q_hid (depth 8)
                                                 |
                                                 v
                                           [t_ble_hid]
```

`t_motion` and `t_app` both write `q_hid`. `t_ble_hid` coalesces consecutive dx/dy deltas and applies the latest button state before sending a single HID report per BLE interval.

## 7. Error handling strategy

Per-layer behaviour on failure:

- `dd_*`: every public function returns a typed error code (e.g., `dd_err_t`). No logging inside drivers; no blocking retries beyond a bounded per-call attempt. Transient I2C NACK returns `DD_ERR_IO`.
- `srv_*`: pure functions. On invalid input (NaN, non-unit quaternion) they log at debug level through an injected logger interface and skip the current frame rather than propagating bad state.
- `app_controller`: owns a software watchdog. If `t_fusion` or `t_ble_hid` misses its expected wake within a configured window, the controller logs the stall, resets the affected driver (I2C bus reset for MPU6050, BLE stack teardown and re-advertise for HID), and resumes. Persistent failures surface as a non-fatal "degraded" state.
- `main.cpp`: no error handling beyond invoking `app_controller_start()`; fatal faults are caught by ESP32 panic handler and trigger a reboot.

Phase III fault-safety (progressive degradation, user-visible fault signalling) is backlog; see ADR-007.

## 8. Cross-core coordination

- Core 0 runs sensor ingestion and fusion: `t_imu_sample`, `t_fusion`, `t_touch`.
- Core 1 runs policy and radio: `t_motion`, `t_app`, `t_ble_hid`.
- The only cross-core hops are `q_orientation` (core 0 -> core 1) and `q_buttons` (core 0 -> core 1).
- All cross-core communication uses FreeRTOS queues (`xQueueSend` / `xQueueReceive`), which are thread- and ISR-safe. No shared globals, no ad-hoc spinlocks.
- The ISR for the MPU6050 data-ready line (if enabled in a later iteration) uses `xQueueSendFromISR` with a higher-priority-task-woken yield; it does not touch `srv_*` state directly.
- `q_imu` is intentionally same-core (both `t_imu_sample` and `t_fusion` live on core 0) to keep the high-rate 100 Hz hop off the inter-core bus.

---

Related decisions: see `docs/srs/decisions.md` ADR-001 through ADR-007.
