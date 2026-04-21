# Air Glove — Architecture Decision Records (ADR-lite)

This log captures the Phase I architectural decisions for the Air Glove firmware. Each entry is short and intentionally narrow in scope. Architectural context is in `docs/srs/architecture.md`; broader domain analysis lives in `report/chapters/ch3_proposed_solution.tex` and `report/chapters/system_design.tex`.

---

## ADR-001 — IMU is MPU6050

- **Status:** Accepted
- **Date:** 2026-04-21
- **Context:** We need a low-cost 6-axis IMU for a wearable glove with I2C wiring and hobbyist-grade availability. Candidates were MPU6050 (6-axis, no onboard magnetometer, no modern DMP toolchain), BNO055 (9-axis with onboard sensor fusion), and ICM-20948 (9-axis with DMP). Budget and parts availability are the hard constraints for Phase I.
- **Decision:** Use the MPU6050 over I2C at 400 kHz on GPIO21/GPIO22, with fusion performed in firmware (see ADR-002).
- **Consequences:** Positive — cheap, widely stocked, well-understood register map, small footprint on the glove. Negative — no onboard fusion, so we own the fusion code and its CPU cost; no magnetometer, so absolute heading drift is accepted within Phase I. Trade-off accepted.

## ADR-002 — Sensor fusion is Madgwick, 100 Hz, quaternion output

- **Status:** Accepted
- **Date:** 2026-04-21
- **Context:** The MPU6050 gives raw accel/gyro, so orientation must be computed on the ESP32. Candidates were Mahony (cheaper, simpler, slower to converge), Madgwick (moderate cost, good convergence, quaternion native), and a full Kalman/EKF (best accuracy, highest complexity and CPU cost).
- **Decision:** Implement Madgwick in `srv_fusion`, running at 100 Hz (10 ms IMU tick), emitting a unit quaternion on `q_orientation`.
- **Consequences:** Positive — quaternion output integrates cleanly with motion mapping, estimated <2 ms per update at 240 MHz leaves ample timing margin against the NFR-LAT-001 <=100 ms budget. Negative — beta gain needs empirical tuning per unit; without a magnetometer yaw is still drift-prone. Accepted for Phase I.

## ADR-003 — Finger sensing uses ESP32 native capacitive touch

- **Status:** Accepted
- **Date:** 2026-04-21
- **Context:** Click detection needs a reliable thumb-to-finger contact signal on a glove without mechanical parts that catch on fabric. Candidates were ESP32 native capacitive touch (`touchRead`), a conductive-thread circuit-closure scheme (thread on thumb closes circuit on each pad), and micro tactile switches sewn into the fingertips.
- **Decision:** Use ESP32 native capacitive touch on channels T0 (thumb reference), T2 (index), T3 (middle), T4 (ring), wrapped in `dd_touch` and debounced in `srv_input`.
- **Consequences:** Positive — no moving parts, minimal wiring, uses the SoC peripheral already available. Negative — readings drift with skin moisture, glove wear, and temperature, so per-boot baseline calibration and threshold tuning are required. Phase I ships with fixed thresholds in `app_config`; adaptive calibration is backlog.

## ADR-004 — BLE stack is NimBLE-Arduino

- **Status:** Accepted
- **Date:** 2026-04-21
- **Context:** The firmware needs a standard BLE HID mouse profile with pairing, bonding, and auto-reconnect. On ESP32 the realistic choices are Bluedroid (default Arduino ESP32 stack, large flash and RAM footprint) and NimBLE via the NimBLE-Arduino port (actively maintained, significantly lighter).
- **Decision:** Use NimBLE-Arduino in `dd_ble_hid` to expose HID mouse advertising, pairing, and `send_report(dx, dy, buttons)`.
- **Consequences:** Positive — smaller flash and heap usage, leaves headroom for fusion code and future features, active upstream. Negative — API differs from Bluedroid examples most tutorials use, so the team pays a small learning-curve cost. Trade-off accepted.

## ADR-005 — Strict three-layer architecture with stdio-only main

- **Status:** Accepted
- **Date:** 2026-04-21
- **Context:** Without a layering rule, hardware headers (`Arduino.h`, `Wire.h`, `BLEDevice.h`) leak throughout the codebase, blocking host-based unit testing and making the firmware hard to reason about. We want to run fusion and motion math in the PlatformIO `native` environment on the developer host.
- **Decision:** Enforce three layers — `app_*`, `srv_*`, `dd_*` — under `air-glove/lib/`, with `dd_*` as the only layer allowed to include Arduino or ESP-IDF headers. `src/main.cpp` uses `<stdio.h>` only and calls `app_controller_start()`.
- **Consequences:** Positive — `srv_*` libraries are unit-testable on the host, the dependency direction is obvious in code review, and swapping hardware (e.g., a different IMU in Phase II) only touches `dd_*`. Negative — a small amount of interface boilerplate (dependency-injected logger, clock, HID sink) is required to keep `srv_*` pure. Worth it.

## ADR-006 — Concurrency is FreeRTOS with pinned tasks plus queues

- **Status:** Accepted
- **Date:** 2026-04-21
- **Context:** The firmware has a high-rate sensor path (IMU at 100 Hz), a parallel touch path, and a radio path whose timing is driven by the BLE connection interval. A single-threaded super-loop or event-loop cannot exploit the ESP32's dual cores and would couple radio jitter into fusion timing.
- **Decision:** Use FreeRTOS with six pinned tasks (see `docs/srs/architecture.md` section 4). Inter-task communication uses FreeRTOS queues only; sensor ingestion and fusion are pinned to core 0, policy and BLE to core 1. No shared globals.
- **Consequences:** Positive — fusion timing is isolated from BLE stack work, dual cores are actually used, queues make ISR integration straightforward and thread-safe. Negative — more tasks means more stack memory and more places to get priority inversion wrong; we accept this cost and mitigate with priority assignments and soak-test stack high-water checks.

## ADR-007 — Phase I MVP scope

- **Status:** Accepted
- **Date:** 2026-04-21
- **Context:** The SRS (`report/chapters/requirements.tex`) splits features across three phases. Trying to implement everything at once risks shipping none of it. We need a clear cut for the first working prototype.
- **Decision:** Phase I delivers BLE pairing with auto-reconnect, pointer XY motion derived from IMU orientation, and left/right click from capacitive touch pads. Scroll gesture, drag/clutch mode, onboard calibration UX, adaptive touch thresholds, and progressive fault-safety are deferred to Phase II and Phase III.
- **Consequences:** Positive — a demonstrable end-to-end prototype is reachable on the current timeline, and the architecture supports the deferred features without restructuring. Negative — Phase I users see no scroll and no visual fault signalling; thresholds that drift with glove wear will need manual re-flashing until Phase II lands. Explicitly accepted.
