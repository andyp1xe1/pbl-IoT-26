# E05 — BLE HID Driver (dd_ble_hid)

- **Status:** In Progress (implementation complete; on-target acceptance pending hardware bring-up)
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

- [~] Windows 10+, Ubuntu 22.04+, macOS 12+ all recognise the device as a plain Bluetooth mouse without driver install (TC-NFR-HID-001). *Standard BLE HID descriptor (4 bytes, no Report ID) + `setAppearance(0x03C2)` + just-works pairing (`setSecurityAuth(bonding=true, mitm=false, sc=true)`, `BLE_HS_IO_NO_INPUT_OUTPUT`) implemented. Manual HIL verification per `docs/plans/09-integration-and-bringup.md` pending.*
- [~] After initial pairing, reboot + connect cycle completes within 5 s without user action (TC-FR02). *NimBLE persists bonding in NVS automatically; driver restarts advertising on `onDisconnect`. Hardware verification pending.*
- [~] `dd_ble_hid_send()` returns in < 2 ms average on ESP32 @ 240 MHz; transport delivery observed < 30 ms typical. *Hot path is one `setValue(buf, 4)` + `notify()` — expected ≪ 2 ms. On-target latency bench per plan 04 "Verification / Latency microbench" pending.*
- [~] Sending at 125 Hz does not deadlock or drop more than 5 % of reports. *Connection-parameter update requests 7.5–15 ms interval (`updateConnParams(…, 6, 12, 0, 200)`) so a 125 Hz producer fits inside the connection cadence. Soak verification pending.*
- [x] Driver exposes zero NimBLE types on its public header — caller sees only the interface from E02. *Verified: `gcc -std=c11 -Werror -fsyntax-only` and `g++ -std=gnu++17 -Werror -fsyntax-only` both compile `dd_ble_hid.h` stand-alone; `grep -E "NimBLE|ble_gap|ble_hs_"` in the header returns only comment lines, no type or include references.*

## Dependencies

- E01, E02.

## Progress log

- 2026-04-21: Epic created. NimBLE-Arduino selected per `docs/srs/decisions.md` ADR-004. Plan: `docs/plans/04-dd-ble-hid.md`.
- 2026-04-21: Implementation delivered. New / updated files:
  - `air-glove/lib/dd_ble_hid/src/ble_hid_report_map.h` — inlined 4-byte HID mouse descriptor (3 buttons + int8 dx/dy/wheel, no Report ID).
  - `air-glove/lib/dd_ble_hid/src/dd_ble_hid.cpp` — NimBLE-backed driver. `dd_ble_hid_init` sets up just-works bonding (`setSecurityAuth(true, false, true)` + `BLE_HS_IO_NO_INPUT_OUTPUT`), creates the `NimBLEHIDDevice`, publishes the report map, and starts advertising with appearance `0x03C2` (HID Mouse). Connect callback requests a tight connection interval (7.5–15 ms, supervision 2 s). Disconnect callback flips `s_connected=false` and re-advertises.
  - `air-glove/test/test_dd_ble_hid/test_init.cpp` — five on-target Unity tests: init returns OK, init is idempotent, not-connected-at-boot, `dd_ble_hid_send(nullptr)` returns `AG_ERR_ARG`, send-while-disconnected is a silent `AG_OK`.
  - `air-glove/platformio.ini` — replaced the earlier `build_src_filter = -<../lib/…>` kludge for `env:native` with the documented mechanism: `lib_ignore` for every hardware-dependent lib (`dd_mpu6050`, `dd_touch`, `dd_ble_hid`, `app_controller`), plus `build_src_filter = +<*> -<main.cpp>` so `src/main.cpp` (which calls FreeRTOS-backed code) is not linked on the host. Added `lib_deps = h2zero/NimBLE-Arduino@^1.4.0` to `env:esp32dev` only.
- 2026-04-21: Verification — public header `dd_ble_hid.h` compiles stand-alone with `-Wall -Wextra -Werror -fsyntax-only` in both C11 and C++17; `grep` confirms the NimBLE type surface does not escape the header (hits are all in comments). `srv_*` libs still compile clean in native mode (no NimBLE reachable through includes). Full NimBLE stack compile is not shim-tested locally — NimBLE's API surface is too large for a meaningful host shim. `pio run -e esp32dev` + hardware bring-up per plan 04 / plan 09 is the remaining gate to flip Done.
