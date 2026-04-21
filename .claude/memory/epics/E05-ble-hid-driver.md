# E05 — BLE HID Driver (dd_ble_hid)

- **Status:** Not Started
- **Phase:** I
- **Owns:** `air-glove/lib/dd_ble_hid/`
- **Plan:** `docs/plans/04-dd-ble-hid.md`
- **Realises:** FR-001, FR-002, FR-010, NFR-HID-001, NFR-LAT-001 (transport half)

## Goal

Expose the ESP32 as a standard BLE HID mouse using the NimBLE-Arduino stack. Handle advertising, pairing (bonding), auto-reconnect to the last host, and transport of `hid_mouse_report_t` frames. Host OS must see a conventional pointing device — no custom drivers.

## Scope

**In:**
- NimBLE init with bonding enabled.
- Standard BLE HID mouse report descriptor: 3 buttons (L, R, M) + dx + dy + wheel (all signed 8-bit).
- Advertise as `"AirGlove"` (configurable via `app_config`).
- Persist bonding keys in NVS so reconnects work after reboot.
- `dd_ble_hid_send()` posts a report; callers may call at up to 125 Hz (the host connection interval caps actual throughput around 7.5–30 ms typical).
- `dd_ble_hid_is_connected()` for higher-level throttling.

**Out:**
- Keyboard or gamepad profiles — mouse only.
- Multi-host switching — Phase II candidate.
- OTA firmware update — Phase III.

## Public interface

See E02 (`dd_ble_hid.h`).

## Acceptance criteria

- [ ] Windows 10+, Ubuntu 22.04+, macOS 12+ all recognise the device as a plain Bluetooth mouse without driver install (TC-NFR-HID-001).
- [ ] After initial pairing, reboot + connect cycle completes within 5 s without user action (TC-FR02).
- [ ] `dd_ble_hid_send()` returns in < 2 ms average on ESP32 @ 240 MHz; transport delivery observed < 30 ms typical over NimBLE default connection interval.
- [ ] Sending at 125 Hz does not deadlock or drop more than 5 % of reports.
- [ ] Driver exposes zero NimBLE types on its public header — caller sees only the interface from E02.

## Dependencies

- E01, E02.

## Progress log

- 2026-04-21: Epic created. NimBLE-Arduino selected per `docs/srs/decisions.md` ADR-004. Plan: `docs/plans/04-dd-ble-hid.md`.
