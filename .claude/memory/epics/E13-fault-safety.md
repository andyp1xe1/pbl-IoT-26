# E13 — Fault Safety  [BACKLOG]

- **Status:** Backlog
- **Phase:** III
- **Realises:** FR-009, FR-010 (hardening half)

## Goal

Make the firmware tolerate transient faults without requiring a manual reset: I²C stalls, IMU NaN/stuck registers, BLE disconnect storms, task stalls. Implement watchdog + recovery strategies per layer.

## Scope (bullets — not yet broken down)

- ESP32 task watchdog: subscribe every task with a reasonable timeout; task stall logs + resets.
- `dd_mpu6050`: detect stuck readings (identical raw sample > N consecutive reads) and re-init I²C.
- `dd_ble_hid`: exponential-backoff re-advertise on disconnect.
- `srv_fusion`: reject NaN/Inf quaternions; reset filter on detection.
- Cross-OS HID hardening: handle host-initiated disconnect, bonding key loss, MTU renegotiation.
- Error telemetry: accumulate counters exposed via `printf` heartbeat.

## Promotion criteria

After E09 acceptance. Prioritise after E10–E12 unless bring-up surfaces stability blockers.

## Progress log

- 2026-04-21: Epic stub created.
