# Air Glove — Glossary

Compact reference of IDs, names, and acronyms used across this project's docs and code.

## Layer prefixes (code & docs)

| Prefix | Meaning | Allowed includes | Example folders |
|--------|---------|------------------|-----------------|
| `dd_`  | **Device driver** — thin hardware wrapper. Only layer permitted to include Arduino.h or ESP-IDF drivers. | Arduino.h, driver/i2c.h, NimBLEDevice.h, driver/touch_sensor.h, etc. | `dd_mpu6050`, `dd_touch`, `dd_ble_hid` |
| `srv_` | **Service** — pure domain logic. Unit-testable on native host. | stdlib only; no Arduino, no ESP-IDF. | `srv_fusion`, `srv_motion`, `srv_input` |
| `app_` | **Application** — orchestration, state, task wiring. Uses services. | services + FreeRTOS primitives (for task/queue glue). | `app_controller`, `app_config` |

## FreeRTOS tasks (Phase I)

| Task | Period | Core | Stack | Priority |
|------|--------|------|-------|----------|
| `t_imu_sample` | 10 ms | 0 | 2 KB | high |
| `t_fusion` | event-driven | 0 | 4 KB | med-high |
| `t_touch` | 10 ms | 0 | 2 KB | med |
| `t_motion` | event-driven | 1 | 4 KB | med |
| `t_app` | event-driven | 1 | 3 KB | med |
| `t_ble_hid` | event-driven | 1 | 4 KB | high |

## FreeRTOS queues

| Queue | Producer → Consumer | Item type | Depth |
|-------|---------------------|-----------|-------|
| `q_imu` | `t_imu_sample` → `t_fusion` | raw IMU sample | 4 |
| `q_orientation` | `t_fusion` → `t_motion` | quaternion + timestamp | 2 |
| `q_buttons` | `t_touch` → `t_app` | button event | 8 |
| `q_hid` | `t_motion` / `t_app` → `t_ble_hid` | HID report | 8 |

## Requirement ID scheme

| Prefix | Meaning | Example |
|--------|---------|---------|
| `FR-00x` | Functional requirement, three-digit zero-padded, matches LaTeX report. | `FR-003` |
| `NFR-<CAT>-00x` | Non-functional requirement; category is `LAT`, `STAB`, `PWR`, `HID`, `ERG`, `MOD`. | `NFR-LAT-001` |
| `Ezz` | Epic ID, two-digit zero-padded, never renumbered. | `E06` |
| `TC-<REQ>-nn` | Test case tied to a requirement. | `TC-FR03-01` |
| `ADR-00x` | Architecture Decision Record, see `decisions.md`. | `ADR-001` |

## Acronyms

| Term | Expansion |
|------|-----------|
| ADR | Architecture Decision Record |
| BLE | Bluetooth Low Energy |
| BOM | Bill of Materials |
| DMP | Digital Motion Processor (on-chip MPU fusion — **not used**, we fuse in firmware) |
| DoA | Definition of Acceptance |
| FR  | Functional Requirement |
| HID | Human Interface Device (USB/BLE mouse/keyboard class) |
| HIL | Hardware-in-the-loop testing |
| I²C | Inter-Integrated Circuit (two-wire bus) |
| IMU | Inertial Measurement Unit |
| MCU | Microcontroller Unit |
| NFR | Non-Functional Requirement |
| NimBLE | Open-source BLE stack; `NimBLE-Arduino` port is used here |
| OTA | Over-the-air firmware update |
| PIO | PlatformIO (build/test system) |
| RTOS | Real-Time Operating System (here: FreeRTOS as shipped with ESP32 Arduino core) |
| SRS | Software Requirements Specification |
| TC | Test Case |

## File path conventions

- All paths in docs use forward slashes (`air-glove/lib/dd_mpu6050/...`).
- PlatformIO project root is `air-glove/` — all `pio` commands run from there.
- Docs are never placed inside `air-glove/` — keep `air-glove/` clean as the build tree.
