# Air Glove — Companion App

A browser (Web Bluetooth) companion for the Air Glove. It connects to the glove's
**custom config/telemetry GATT service** to tune motion parameters, view live
sensor telemetry, trigger calibration, and read device info.

It does **not** handle mouse pairing — the glove is a standard BLE HID mouse and
the operating system owns that pairing. This app only opens a separate GATT
connection to the configuration service.

See `../docs/plans/10-companion-app.md` for the full spec and the GATT contract.

## Run

```bash
npm install
npm run dev
```

Open the printed `http://localhost:5173` URL in **Chrome or Edge** (desktop or
Android). Web Bluetooth requires a *secure context*: `localhost` is fine; any
other host must be served over `https://`.

## Browser support

- Chrome 56+ / Edge 79+ (desktop, Android): full support.
- Safari / iOS: **no Web Bluetooth** — the app auto-falls back to a built-in
  **mock client** so the UI is still fully explorable (synthetic telemetry,
  in-memory config, scripted calibration). The mock is also handy for developing
  and demoing without hardware.

## Layout

- `src/ble/uuids.ts` — canonical GATT UUIDs (must match firmware).
- `src/ble/codec.ts` — packed config/telemetry/status encode/decode (LE).
- `src/ble/client.ts` — real Web Bluetooth client.
- `src/ble/mockClient.ts` — hardware-free mock implementing the same interface.
- `src/state/store.ts` — app state + connection lifecycle.
- `src/screens/` — Connect, Tune, Calibrate, About.

## Test

```bash
npm test          # vitest: codec round-trip + fixtures
npm run typecheck
```
