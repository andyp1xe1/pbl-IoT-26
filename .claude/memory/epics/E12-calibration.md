# E12 — Calibration  [BACKLOG]

- **Status:** Backlog
- **Phase:** II
- **Realises:** FR-008

## Goal

User-triggerable calibration of neutral glove orientation and IMU bias. Persist across reboots. Eliminates "I put the glove on slightly differently each session" drift.

## Scope (bullets — not yet broken down)

- Trigger: held chord (e.g., thumb+index+middle for 2 s) while the user holds the glove in their preferred neutral pose.
- Record averaged gyro bias (500 samples) + neutral quaternion.
- Subtract gyro bias in `dd_mpu6050` on read, or in `srv_fusion` at the top of each update (TBD).
- Store in NVS under namespace `airglove/calib`.
- Emit feedback (brief cursor animation) on success/failure.

## Promotion criteria

After E09 acceptance. Likely bundled with E10 (both need NVS + chord-active semantics).

## Progress log

- 2026-04-21: Epic stub created.
