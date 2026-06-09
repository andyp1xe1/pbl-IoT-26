# Plan 10 — Companion App (Web Bluetooth Config & Telemetry Client)

- **Epic:** E15 (proposed — see "Upstream scope changes" below; epic stub not yet created).
- **Goal:** Build a browser-based companion app (Web Bluetooth) that connects to the glove over a **new custom GATT config/telemetry service** — separate from the HID mouse profile — to let a user tune motion parameters, view live sensor telemetry, trigger calibration, and read device info. The HID mouse path is untouched; the OS still owns HID pairing.
- **Preconditions:**
  - Plans 01–09 complete; a glove that boots, advertises as "AirGlove", and works as a BLE HID mouse.
  - Firmware extended to expose the GATT contract in §"GATT contract" below. **This firmware work does not exist yet** — it is a hard precondition for any *real* (non-mock) function. The web app can be built and demoed against a software mock before the firmware lands.
  - A host with Chrome 56+ or Edge 79+ (desktop or Android). **Web Bluetooth is not available on iOS/Safari** — accepted per the platform decision (desktop Windows is the primary host).
  - The page is served from a **secure context**: `https://` or `http://localhost`. Web Bluetooth refuses to run otherwise.

## Why this design (the two hard constraints)

1. **The app cannot perform HID pairing.** The glove is a BLE HID mouse (`docs/plans/04-dd-ble-hid.md`); pairing/bonding is owned by the OS Bluetooth settings, and `TC-NFR-HID-001` depends on it staying a plain mouse. Web Bluetooth additionally **cannot access HID at all** (HID is on the Web Bluetooth blocklist). Therefore the "Pair" screen becomes a **Connect** screen: a one-time instruction to pair the mouse in OS settings, plus a button that opens `navigator.bluetooth.requestDevice` to connect to the *config* service (a normal GATT connection, independent of the HID bond).
2. **Tune/Calibrate need something to talk to.** Today, sensitivity/deadzone/mapping are compile-time constants in `app_config`. For the app to do anything real, the firmware must expose a writable GATT service. That service is specified below and is the shared contract between firmware and app.

## Files to create

App lives in a new top-level folder `companion/` (sibling to `air-glove/`), so the firmware build is untouched.

| Path | Role |
|------|------|
| `companion/package.json` | Vite + React + TypeScript app metadata and scripts. |
| `companion/index.html` | App entry; secure-context note in a comment. |
| `companion/vite.config.ts` | Vite config; dev server bound to `localhost` (secure context for Web Bluetooth). |
| `companion/tsconfig.json` | TS config. |
| `companion/src/main.tsx` | React root. |
| `companion/src/App.tsx` | Shell + bottom tab navigation (Connect / Tune / Calibrate / About). |
| `companion/src/ble/uuids.ts` | Single source of truth for all GATT UUIDs (must match firmware). |
| `companion/src/ble/codec.ts` | Encode/decode the packed config & telemetry structs (little-endian). |
| `companion/src/ble/AirGloveClient.ts` | Web Bluetooth wrapper: connect, read/write config, subscribe telemetry/status, send commands, read DIS/battery. |
| `companion/src/ble/mockClient.ts` | Software mock implementing the same interface — lets the UI run with no hardware (demo + dev). |
| `companion/src/state/store.ts` | App state (connection status, config, telemetry, calibration status). |
| `companion/src/screens/ConnectScreen.tsx` | OS-pairing guide + "Connect" button + connection status / battery. |
| `companion/src/screens/TuneScreen.tsx` | Sensitivity X/Y, deadzone sliders, click mapping, live touch bars, Save. |
| `companion/src/screens/CalibrateScreen.tsx` | Live IMU readout, step procedure, "Calibrate IMU" + "Recalibrate touch" buttons, progress. |
| `companion/src/screens/AboutScreen.tsx` | Firmware/model/manufacturer (from DIS), app version, links. |
| `companion/src/ui/*` | Small presentational components (Slider, StatBox, TouchBar, TabBar) styled after the mockup. |
| `companion/README.md` | How to run (`npm i && npm run dev`), browser support, secure-context note, mock vs real toggle. |

## GATT contract (shared firmware ⇄ app)

This is the authoritative interface. Firmware MUST implement it exactly; the app MUST NOT diverge. All multi-byte fields are **little-endian**.

### Custom base UUID

`4147xxxx-7a13-4b1e-9c2f-1d0e5f6a7b8c` — the leading `4147` is ASCII "AG". The 16-bit `xxxx` field selects service/characteristic.

### Air Glove Config Service — `41470001-…`

| Characteristic | UUID `xxxx` | Props | Payload |
|----------------|-------------|-------|---------|
| Config | `0002` | Read, Write | `ag_config_t` (10 bytes) — atomic read/write of all tunables. |
| Telemetry | `0003` | Read, Notify | `ag_telemetry_t` (24 bytes) — live sensor frame, notified ~10–20 Hz. |
| Command | `0004` | Write (with response) | 1-byte opcode + optional payload. |
| Status | `0005` | Read, Notify | `ag_status_t` (4 bytes) — calibration/command progress + result. |

`ag_config_t` (10 bytes):

```
offset size field            units / notes
0      u8   version          = 1
1      u8   flags            bit0 = config dirty (unsaved); reserved otherwise
2      u16  sens_x_milli     sensitivity X ×1000 (e.g. 1400 = 1.40×)
4      u16  sens_y_milli     sensitivity Y ×1000
6      u16  deadzone_mrad    deadzone in milli-radians (e.g. 80 = 0.08 rad)
8      u8   click_map        0 = idx→L / mid→R (default); 1 = swapped
9      u8   reserved         = 0
```

`ag_telemetry_t` (24 bytes):

```
offset size field            units / notes
0      u8   version          = 1
1      u8   seq              wraps 0..255, lets the app detect dropped notifications
2      i16  accel_x/y/z      milli-g  (3 × i16 = 6 bytes)
8      i16  gyro_x/y/z       milli-dps (3 × i16 = 6 bytes)
14     u16  touch[4]         raw touchRead values: thumb,index,middle,ring (8 bytes)
22     u8   battery_pct      0..100
23     u8   flags            bit0 = HID host connected; bit1 = calibrating
```

`ag_status_t` (4 bytes):

```
offset size field            notes
0      u8   last_opcode      echoes the command being reported on
1      u8   state            0 idle, 1 running, 2 success, 3 fail
2      u8   progress         0..100
3      u8   reserved
```

Command opcodes (Command characteristic):

```
0x01  start IMU calibration  (glove flat & still; firmware averages gyro bias)
0x02  recalibrate touch baseline
0x03  save config to NVS     (persist current ag_config_t)
0x04  factory-reset config   (restore compile-time defaults)
```

### Reused standard services (no custom code needed on the app side beyond reads)

- **Device Information Service** `0x180A` — Manufacturer Name, Model Number, Firmware Revision → About screen.
- **Battery Service** `0x180F` — Battery Level (0–100) → Connect screen badge (also mirrored in telemetry for convenience).

> Web Bluetooth note: both `device_information` and `battery_service` are allowed (not blocklisted) but must be declared in `optionalServices` on `requestDevice`. The custom `41470001-…` service must also be listed there (or used as a filter).

## Step-by-step

1. **Scaffold.** `npm create vite@latest companion -- --template react-ts`; add it as a sibling of `air-glove/`. Add `@types/web-bluetooth` as a dev dependency for typings. Keep dependencies minimal (no UI kit; hand-roll the few components to match the mockup).
2. **Freeze UUIDs.** Author `src/ble/uuids.ts` with the constants from the contract above. This file is the canonical list — when the firmware epic starts, it imports the same values (copy into a firmware header).
3. **Codec.** In `src/ble/codec.ts` implement `encodeConfig`, `decodeConfig`, `decodeTelemetry`, `decodeStatus` over `DataView` with explicit `littleEndian = true`. Unit-test the round-trip (`encodeConfig(decodeConfig(x)) === x`).
4. **Client interface.** Define a TS interface `IAirGloveClient` (connect, disconnect, onConnectionChange, readConfig, writeConfig, sendCommand, subscribeTelemetry, subscribeStatus, readDeviceInfo, readBattery). Implement it twice: `AirGloveClient` (real Web Bluetooth) and `mockClient` (timers feeding synthetic telemetry, in-memory config). A build-time/UI flag selects which one — so the app demos without hardware.
5. **Real client — connect.** `navigator.bluetooth.requestDevice({ filters: [{ services: [CONFIG_SERVICE] }], optionalServices: [CONFIG_SERVICE, '0000180a-…', '0000180f-…'] })` → `device.gatt.connect()` → cache the service + characteristic handles. Wire `gattserverdisconnected` to update state and offer reconnect.
6. **Telemetry subscription.** `char.startNotifications()` + `characteristicvaluechanged` → `decodeTelemetry` → push into store. Throttle UI updates to ~15 Hz (telemetry may arrive faster) and use `seq` to surface a dropped-frame indicator.
7. **State store.** `src/state/store.ts` (Zustand or a small context+reducer): `{ status: 'disconnected'|'connecting'|'connected', config, telemetry, calStatus, deviceInfo }`. Screens subscribe to slices.
8. **Connect screen.** Render the OS-pairing instruction card ("Open Bluetooth settings, pair *AirGlove*, then connect here"), a Connect button (calls `client.connect()`), and once connected show battery + a "Forget/Disconnect" action. This replaces the mockup's "scan & pair" list — Web Bluetooth uses the browser's own device-chooser dialog, so the app does not render a custom scan list.
9. **Tune screen.** Build sliders for Sensitivity X, Sensitivity Y, Deadzone, and a segmented control for click mapping, bound to `config`. Writes are **debounced** (~150 ms) to `writeConfig`; show the live "TOUCH — LIVE" bars from telemetry exactly like the mockup, with a draggable click-threshold marker (visual only in Phase A; wiring a threshold characteristic is a follow-up). A **Save** button issues command `0x03` (persist to NVS) and reflects the `flags.dirty` bit.
10. **Calibrate screen.** Show the live IMU grid (accel/gyro from telemetry) and the 3-step procedure card from the mockup. "Calibrate IMU" → command `0x01`; subscribe to Status and render the progress bar; show success/fail. "Recalibrate touch baseline" → command `0x02`. If firmware calibration is not yet implemented, the mock returns a scripted success so the UX is demonstrable.
11. **About screen.** Read DIS strings + app version (from `package.json` via Vite `import.meta.env`); list known limitations (iOS unsupported, single-host).
12. **Styling.** Match the mockup: iOS-style grouped sections, system font stack, blue accent (`#0a84ff`), light-gray grouped background, bottom tab bar (Connect / Tune / Calibrate / About). Make it responsive so it reads well on a phone-width viewport *and* a desktop window (since the primary host is the Windows laptop).
13. **README.** Document `npm run dev` (serves on `localhost`, a secure context), Chrome/Edge requirement, the mock toggle, and the "pair in OS first" step.

## Internal design notes

- **Two independent links.** The HID bond (OS-owned) and the GATT config connection (app-owned) coexist on the one NimBLE server. The app never sees HID. Disconnecting the app does not drop the mouse.
- **Atomic config.** A single packed `ag_config_t` read/write avoids partial-update races vs. many tiny characteristics. The Save command separates "live preview" (RAM) from "persist" (NVS) so users can experiment without wearing NVS.
- **Telemetry sizing.** 24 bytes fits comfortably under the default 23-byte ATT MTU minus 3-byte header? No — 24 > 20 usable. The firmware MUST negotiate a larger MTU (NimBLE default 247) on connect, or the app must request `device.gatt` MTU is fixed by the platform. To stay safe, telemetry notifications rely on the negotiated MTU; if a host caps MTU at 23, split telemetry into two characteristics. Document the MTU assumption in the firmware epic and verify on each browser.
- **Secure context.** Vite dev server on `http://localhost` qualifies. For LAN/phone testing, serve over `https` (e.g. a self-signed cert or a tunneling tool) — plain `http://<lan-ip>` will be rejected by Web Bluetooth.
- **No background scanning.** Web Bluetooth requires a user gesture to open the chooser; there is no silent reconnect to a *new* device. For a previously granted device, `navigator.bluetooth.getDevices()` (where supported) can enable a one-click reconnect — treat as progressive enhancement.

## Verification

- **Codec unit tests** (`vitest`): config round-trip; telemetry decode against a fixed byte fixture; status decode.
- **Mock-mode walkthrough:** with the mock client, exercise all four screens — sliders update config, Save flips dirty→clean, Calibrate runs to success, About shows stub info. No hardware needed; this is the demo-safe path.
- **Real-hardware HIL** (once firmware lands):
  1. Pair the glove as a mouse in OS settings; confirm cursor works (HID untouched).
  2. Open the app in Chrome/Edge → Connect → select AirGlove in the browser chooser → state shows "connected".
  3. Move sensitivity slider → cursor feel changes live; Save → power-cycle glove → setting persists (proves NVS write).
  4. Pinch fingers → touch bars react in real time; tilt glove → IMU grid updates.
  5. Run Calibrate IMU flat & still → status reaches success; at-rest drift improves (cross-check `09-integration-and-bringup.md` §8).
- **Browser matrix:** Chrome (Windows), Edge (Windows), Chrome (Android). Record iOS as "Not Supported" (expected).

## Rollback / risk

- **Primary risk:** Web Bluetooth platform quirks (MTU caps, chooser behaviour, `getDevices` availability) differ across Chrome/Edge/Android. Mitigation: the `IAirGloveClient` seam keeps all platform code in one file; the mock client guarantees the UI always demos.
- **Firmware-not-ready risk:** the entire *real* value depends on the new GATT service. Mitigation: ship the app against the mock first; the GATT contract here is the spec the firmware epic implements, so the two can proceed in parallel.
- **Scope risk:** this feature is currently **out of scope** (see below). Do not start firmware work before the upstream docs are updated and the epic is promoted.
- **Rollback step:** the app is an isolated `companion/` folder with no coupling to `air-glove/`; deleting it has zero firmware impact. The firmware-side config service should live behind a build flag so it can be compiled out.

## Upstream scope changes (do these first, per the top-down change flow)

`docs/plans/00-overview.md` mandates: a new/changed FR → owning epic → owning plan → code. This plan therefore depends on these edits, which are **not yet made**:

1. **`.claude/memory/requirements.md`** — remove (or qualify) the "Companion mobile app" out-of-scope line; add **FR-011 "Companion app for runtime configuration & telemetry over a custom GATT service"** and map it to the new epic.
2. **New epic `.claude/memory/epics/E15-companion-app.md`** — owns the GATT config/telemetry service (firmware side) and the web client (app side). Relates to E11 (sensitivity presets) and E12 (calibration), which this app exposes a UI for.
3. **`docs/srs/decisions.md`** — add **ADR-008 "Companion app is a Web Bluetooth client over a custom GATT config service (not native mobile, not via HID)"**, recording: HID stays OS-paired; config is a separate GATT service; iOS unsupported by Web Bluetooth; desktop Chrome/Edge is the target.

## References

- Plan 04 (`docs/plans/04-dd-ble-hid.md`) — HID profile, NimBLE server, just-works bonding (the link this app must NOT disturb).
- Plan 09 (`docs/plans/09-integration-and-bringup.md`) §8 / §11 — drift + calibration context the Calibrate screen ties into.
- Epics E11 (sensitivity) and E12 (calibration) — the firmware behaviours this app surfaces.
- `.claude/memory/requirements.md` — FR-007, FR-008 (related), and the out-of-scope note this plan revises.
- Web Bluetooth spec & blocklist — verify current Chrome/Edge support and the HID/blocklisted-services list at implementation time.
