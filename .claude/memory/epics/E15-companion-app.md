# E15 — Companion App  [BACKLOG]

- **Status:** Backlog
- **Phase:** II
- **Realises:** FR-011
- **Owns:** `companion/` (web client) + the custom config/telemetry GATT service on the firmware side.
- **Plan:** `docs/plans/10-companion-app.md`
- **Decision:** `docs/srs/decisions.md` ADR-008.

## Goal

Give the user a browser-based app (Web Bluetooth) to configure and observe the glove at runtime — sensitivity X/Y, deadzone, click mapping, live IMU + touch telemetry, calibration triggers, and device info — without re-flashing. The app talks to a **new custom GATT config/telemetry service**, kept entirely separate from the HID mouse profile so OS HID pairing and `TC-NFR-HID-001` are unaffected.

## Scope (bullets — not yet broken down)

**Firmware side (new, behind a build flag):**
- Register a custom config GATT service on the existing NimBLE server (`dd_ble_hid` owns NimBLE init; the config service either extends it or is a sibling `dd_*` lib that receives the server handle — decide at breakdown).
- Characteristics per the contract in `docs/plans/10-companion-app.md`: Config (R/W), Telemetry (R/Notify), Command (W), Status (R/Notify).
- Bridge config values to `app_config`/`motion_config_t` at runtime; persist on the Save command to NVS (overlaps E11/E12 NVS work).
- Wire Command opcodes to calibration (E12) and touch-rebaseline (E04) routines.
- Negotiate an MTU large enough for the 24-byte telemetry frame, or split telemetry.

**App side (`companion/`):**
- Vite + React + TypeScript SPA; four screens (Connect / Tune / Calibrate / About) styled after the existing mockup.
- `IAirGloveClient` seam with a real Web Bluetooth client and a software mock client (demo without hardware).
- Live telemetry via notifications; debounced config writes; explicit Save to persist.

## Relationships

- **E11 (sensitivity switch)** and **E12 (calibration)** define firmware behaviours this app exposes a UI for. E15 should be sequenced with or after them so the Command opcodes have real implementations; until then the mock client demonstrates the UX.
- Depends on E05 (HID driver / NimBLE server) being stable — the config service shares that server.

## Promotion criteria

After E09 acceptance. Coordinate with E11/E12 so the runtime-config + calibration + NVS work is done once. The web app (mock mode) can be built independently at any time.

## Open questions

- Firmware ownership: extend `dd_ble_hid` vs. a new `dd_ble_cfg` lib sharing the NimBLE server.
- MTU strategy across Chrome/Edge/Android (single 24-byte telemetry char vs. split).
- One-click reconnect via `navigator.bluetooth.getDevices()` — progressive enhancement, browser-dependent.

## Progress log

- 2026-06-08: Epic stub created. Plan `docs/plans/10-companion-app.md` authored; FR-011 added to requirements; ADR-008 recorded. Decision: web (Web Bluetooth) over native; iOS unsupported accepted.
