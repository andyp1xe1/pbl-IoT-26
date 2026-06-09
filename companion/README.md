# Air Glove — Companion App

Browser (Web Bluetooth) tuner for the Air Glove. Connects to the glove's custom
config/telemetry GATT service to adjust motion parameters, view live sensor data,
and trigger calibration.

Mouse pairing is owned by the operating system. This app only talks to the
separate configuration service on the same NimBLE server.

## Run

```bash
npm install
npm run dev
```

Open the printed `http://localhost:5173` in Chrome or Edge. Web Bluetooth
requires a secure context (`localhost` qualifies; anything else needs `https://`).

## Layout

- `src/ble/uuids.ts` — canonical GATT UUIDs (must match firmware).
- `src/ble/codec.ts` — v2 packed config/telemetry/status encode/decode (LE).
- `src/ble/client.ts` — Web Bluetooth client.
- `src/state/store.ts` — app state + connection lifecycle.
- `src/screens/` — Connect, Tune, Calibrate, About.

## Test

```bash
npm test          # vitest: codec round-trip + fixtures
npm run typecheck
```
