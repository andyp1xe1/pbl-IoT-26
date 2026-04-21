# Plan 04 — dd_ble_hid (NimBLE HID Mouse Driver)

- **Epic:** E05 (see `.claude/memory/epics/E05-ble-hid-driver.md`)
- **Goal:** Implement `dd_ble_hid` as a NimBLE-Arduino-backed BLE HID mouse transport that advertises as "AirGlove", handles just-works bonding persisted in NVS, and exposes only the C interface sketched in E02 — no NimBLE types leak to callers.
- **Preconditions:**
  - Plan 01 (scaffolding) complete — PlatformIO project exists with `esp32dev` env and the `dd_ble_hid/include/dd_ble_hid.h` header stub from E02 is in place.
  - `app_config/include/ag_types.h` defines `ag_result_t` and `hid_mouse_report_t` per E02.
  - ADR-004 selects NimBLE-Arduino as the BLE stack.

## Files to create

| Path | Role |
|------|------|
| `air-glove/lib/dd_ble_hid/include/dd_ble_hid.h` | Public C header (no NimBLE types). Matches E02 signatures. |
| `air-glove/lib/dd_ble_hid/src/dd_ble_hid.cpp` | Implementation for `esp32dev` — owns all NimBLE objects. |
| `air-glove/lib/dd_ble_hid/src/dd_ble_hid_native.cpp` | Stub implementation for `env:native` (returns OK, `is_connected()` returns false). |
| `air-glove/lib/dd_ble_hid/src/ble_hid_report_map.h` | Static HID report descriptor byte array. |
| `air-glove/lib/dd_ble_hid/library.json` | PlatformIO lib metadata; declares NimBLE-Arduino dependency for `esp32dev` only. |
| `air-glove/test/test_dd_ble_hid/test_init.cpp` | On-target Unity test — init succeeds, advertising starts. |

## Step-by-step

1. **Declare the library dependency.** In `air-glove/lib/dd_ble_hid/library.json` reference `h2zero/NimBLE-Arduino` at `^1.4.0` gated to `platforms: ["espressif32"]`. In `air-glove/platformio.ini` under `[env:esp32dev]` add `lib_deps = h2zero/NimBLE-Arduino@^1.4.0`. Do NOT add it to `[env:native]`. Verify the exact package identifier in the NimBLE-Arduino README if PlatformIO cannot resolve it.
2. **Freeze the public header.** Copy the E02 signatures verbatim into `dd_ble_hid.h` (see "Public header" section below). Guard with `#ifdef __cplusplus extern "C" {`.
3. **Write the HID report descriptor.** Put the byte array in `ble_hid_report_map.h` as a `static const uint8_t kHidReportMap[]`. Use a 4-byte report (no Report ID): 3 buttons (bits 0–2, bit 3–7 padding), int8 dx, int8 dy, int8 wheel. See "Internal design notes" for the hex bytes.
4. **Implement `dd_ble_hid_init(const char *device_name)` in `.cpp`.**
   - Call `NimBLEDevice::init(device_name ? device_name : "AirGlove")`.
   - Call `NimBLEDevice::setPower(ESP_PWR_LVL_P3)` (or leave default; verify constant name in NimBLE-Arduino README).
   - Call `NimBLEDevice::setSecurityAuth(/*bonding=*/true, /*mitm=*/false, /*sc=*/true)` — bonding + secure-connections pairing, MITM off because the glove has no PIN entry. Justification: a mouse is a "just-works" HID peripheral per Bluetooth Core; MITM would require display or keyboard I/O capability the glove lacks.
   - Call `NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT)` to signal just-works.
   - Create a `NimBLEServer`, register connection callbacks (see step 6).
   - Instantiate NimBLE's built-in `NimBLEHIDDevice` helper; set `manufacturer("AirGlove")`, `pnp(0x02, 0xE502, 0xA111, 0x0210)` (USB-IF vendor source + arbitrary product id; values are placeholders — verify nothing collides), `hidInfo(0x00, 0x01)` (country=0, flags=normal+remote-wakeable as appropriate for mouse).
   - Call `hid->reportMap((uint8_t*)kHidReportMap, sizeof(kHidReportMap))`.
   - Grab the input-report characteristic: `g_input = hid->inputReport(0)` (Report ID 0 since our descriptor has none; verify the argument semantics in the NimBLE-Arduino `BLEHIDDevice` example).
   - Call `hid->startServices()`.
   - Start advertising: `NimBLEAdvertising *adv = NimBLEDevice::getAdvertising(); adv->setAppearance(0x03C2 /* HID mouse */); adv->addServiceUUID(hid->hidService()->getUUID()); adv->setScanResponse(true); adv->start();`.
   - `printf("dd_ble_hid advertising: %s\n", device_name);` on success.
   - Return `AG_OK` on success, `AG_ERR_INIT` on any exception/false return.
5. **Implement `dd_ble_hid_send(const hid_mouse_report_t *r)`.**
   - If `r == nullptr` return `AG_ERR_ARG`.
   - If `!g_connected` return `AG_OK` (drop silently; callers check `is_connected()` first).
   - Pack into a 4-byte buffer: `{ r->buttons & 0x07, (uint8_t)r->dx, (uint8_t)r->dy, (uint8_t)r->wheel }`.
   - `g_input->setValue(buf, 4); g_input->notify();`.
   - Return `AG_OK`. Any NimBLE exception → `AG_ERR_IO`.
6. **Connection callback.** Subclass `NimBLEServerCallbacks` in an anonymous namespace inside the `.cpp`. On `onConnect` set `g_connected = true`, request a shorter connection interval: `pServer->updateConnParams(desc->conn_handle, 6, 12, 0, 200)` (7.5–15 ms interval, 2 s supervision timeout; values in 1.25 ms units per Bluetooth spec). On `onDisconnect` set `g_connected = false` and call `NimBLEDevice::startAdvertising()` to resume advertising.
7. **Connection state flag.**
   ```cpp
   static volatile bool g_connected = false;
   ```
   `dd_ble_hid_is_connected()` returns `g_connected`.
8. **Native stub.** In `dd_ble_hid_native.cpp` implement all three entry points trivially: `init` returns `AG_OK`, `send` returns `AG_OK`, `is_connected` returns `false`. This lets `srv_*` tests link in `env:native`.
9. **Wire into `app_controller`.** `app_controller_start()` calls `dd_ble_hid_init("AirGlove")` during the `INIT` phase; no direct NimBLE calls in `app_controller`.

## Public header (target signature)

Verbatim from E02 — do not extend without bumping E02:

```c
// air-glove/lib/dd_ble_hid/include/dd_ble_hid.h
#pragma once
#include <stdbool.h>
#include "ag_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Initialise the BLE stack, register the HID mouse profile, start advertising.
// device_name is advertised as the BLE "Complete Local Name". Pass NULL to use "AirGlove".
// Thread-safety: call once from app_controller before any send().
ag_result_t dd_ble_hid_init(const char *device_name);

// Enqueue one HID mouse report for transmission. Non-blocking. Coalesce upstream.
// If no host is connected the call is a no-op returning AG_OK.
// Thread-safety: safe to call from any FreeRTOS task; NimBLE serialises internally.
ag_result_t dd_ble_hid_send(const hid_mouse_report_t *r);

// Returns true iff a BLE central is currently connected AND the HID profile is active.
// Thread-safety: lock-free read of a volatile bool.
bool dd_ble_hid_is_connected(void);

#ifdef __cplusplus
}
#endif
```

## Internal design notes

### HID report descriptor (4 bytes, no Report ID)

Standard BLE HID mouse descriptor, bit-for-bit compatible with the USB HID mouse boot layout minus the Report ID. Inlined here so the exact bytes are reviewable:

```c
static const uint8_t kHidReportMap[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop)
    0x09, 0x02,        //   Usage (Mouse)
    0xA1, 0x01,        //   Collection (Application)
    0x09, 0x01,        //     Usage (Pointer)
    0xA1, 0x00,        //     Collection (Physical)
    // --- Buttons: 3 bits + 5 pad ---
    0x05, 0x09,        //       Usage Page (Button)
    0x19, 0x01,        //       Usage Minimum (Button 1)
    0x29, 0x03,        //       Usage Maximum (Button 3)
    0x15, 0x00,        //       Logical Min (0)
    0x25, 0x01,        //       Logical Max (1)
    0x95, 0x03,        //       Report Count (3)
    0x75, 0x01,        //       Report Size (1)
    0x81, 0x02,        //       Input (Data, Var, Abs)
    0x95, 0x01,        //       Report Count (1)
    0x75, 0x05,        //       Report Size (5)
    0x81, 0x03,        //       Input (Const) -- padding
    // --- X, Y, Wheel: three int8 axes ---
    0x05, 0x01,        //       Usage Page (Generic Desktop)
    0x09, 0x30,        //       Usage (X)
    0x09, 0x31,        //       Usage (Y)
    0x09, 0x38,        //       Usage (Wheel)
    0x15, 0x81,        //       Logical Min (-127)
    0x25, 0x7F,        //       Logical Max (127)
    0x75, 0x08,        //       Report Size (8)
    0x95, 0x03,        //       Report Count (3)
    0x81, 0x06,        //       Input (Data, Var, Rel)
    0xC0,              //     End Collection
    0xC0               //   End Collection
};
```

Total wire report size: 4 bytes. Byte 0 = buttons (bit 0 L, bit 1 R, bit 2 M); bytes 1–3 = dx, dy, wheel (signed).

### Security / pairing

- `setSecurityAuth(bonding=true, mitm=false, sc=true)` — standard just-works bonding for a mouse. MITM requires a PIN display or keypad; a glove has neither.
- `setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT)` — declares no I/O to the peer; this triggers just-works pairing on all three target OSes.
- NimBLE persists bonding (LTK, IRK, CSRK) in NVS under its own namespace automatically; no app-side NVS work needed. Erase NVS to reset pairings (`nvs_flash_erase()` — but do NOT wire that into production code).

### Multi-host / bond overflow (known limitation)

NimBLE stores a bounded number of bonds (default 3; check the NimBLE-Arduino `CONFIG_BT_NIMBLE_MAX_BONDS`). Once full, pairing with a new host requires forgetting the device on one of the existing hosts, otherwise NimBLE will evict the oldest bond. Multi-host switching (pair-to-host-A, switch-to-host-B at runtime) is not solved in Phase I — document as an open item for the Phase II multi-host epic.

### Power

Per `docs/srs/hardware.md` §5, BLE advertising + HID notification duty adds roughly 80 mA on top of the ESP32 active baseline. Advertising is continuous while disconnected, so an unpaired glove is the worst-case power draw. After connection, the radio spends most of its time in the low-duty slave latency window and the average drops. Aggressive power optimisations (light sleep, connection-parameter renegotiation, selective advertising) are tracked under Phase III epic E14 and intentionally out of scope here.

### Thread-safety

`dd_ble_hid_send()` may be called from `t_ble_hid` (core 1) at up to 125 Hz per E05. NimBLE's internal task serialises notification queueing, so no app-side mutex is needed. `g_connected` is `volatile bool` written only from the NimBLE callback task and read by any caller; torn reads are impossible on ESP32 (32-bit aligned single-byte access).

### Memory footprint

NimBLE-Arduino default configuration is ~50 KB flash and ~15 KB RAM — well inside ESP32-WROOM's budget. The HID service object holds a handful of characteristics (~1 KB heap).

## Verification

### On-target Unity test (`pio test -e esp32dev -f test_dd_ble_hid`)

- `test_init_returns_ok`: call `dd_ble_hid_init("AirGloveTest")`, assert return is `AG_OK`. No host required.
- `test_not_connected_at_boot`: after init, `dd_ble_hid_is_connected()` returns `false`.
- `test_send_when_disconnected_is_ok`: send a zero report, expect `AG_OK` (silent drop).

### Manual HIL — per `docs/srs/testing-strategy.md` §5

**TC-FR01-01 (device init + HID startup):**
1. Cold-boot the glove from USB power.
2. Open the host's Bluetooth settings pane; start a scan.
3. Pass: within 5 s, the host lists a device advertising as "AirGlove"; serial log shows `dd_ble_hid advertising: AirGlove`.

**TC-FR02-01 (pair + auto-reconnect):**
1. Pair to a Windows 10/11 host via the OS dialog (just-works; no PIN).
2. Power off the glove (slide switch or unplug USB); wait 10 s.
3. Power it back on.
4. Pass: the host reconnects automatically within 5 s with no user prompt; cursor input resumes.

**TC-NFR-HID-001 (cross-OS HID compatibility):**
1. Pair the same unmodified firmware image to Windows 10+, Ubuntu 22.04+, and macOS 12+ in sequence.
2. Pass: each OS recognises the device as a plain Bluetooth mouse — no driver install, no warnings.
3. Cursor motion and both clicks work on each host.

### Latency microbench (developer-side)

Wrap `dd_ble_hid_send()` in `esp_timer_get_time()` inside the driver itself:

```cpp
uint64_t t0 = esp_timer_get_time();
g_input->setValue(buf, 4);
g_input->notify();
uint64_t dt = esp_timer_get_time() - t0;
// log dt to a ring buffer, dump over serial on demand
```

Acceptance: p50 < 1 ms, p95 < 2 ms on the device side. Host-visible end-to-end latency (≤ 100 ms p95 per NFR-LAT-001) is measured by the procedure in Plan 09.

## Rollback / risk

- **Primary risk:** NimBLE-Arduino just-works pairing mis-negotiates on a specific OS (most often macOS). Mitigation: try `setSecurityAuth(true, false, false)` to drop secure-connections, or flip `setSecurityIOCap` to a different capability and re-test the full TC-NFR-HID-001 matrix.
- **Fallback plan:** if NimBLE pairing is not workable on one target OS, swap to the Bluedroid `BLEHIDDevice` path. The entire blast radius is contained in this library because the public header has no NimBLE types — `srv_*`, `app_*`, and the other `dd_*` drivers are untouched.
- **Rollback step:** delete `lib/dd_ble_hid/src/dd_ble_hid.cpp`, keep the header, implement a Bluedroid `.cpp` under the same header. No other code changes required.

## References

- ADR-004 (BLE stack = NimBLE-Arduino) — `docs/srs/decisions.md`.
- Epic E05 — `.claude/memory/epics/E05-ble-hid-driver.md`.
- E02 HW abstraction layer (header contract) — `.claude/memory/epics/E02-hw-abstraction-layer.md`.
- Architecture §4 task map (`t_ble_hid` on core 1, highest priority) — `docs/srs/architecture.md`.
- Requirements FR-001, FR-002, FR-010, NFR-LAT-001, NFR-HID-001 — `.claude/memory/requirements.md`.
- Testing TC-FR01-01, TC-FR02-01, TC-NFR-HID-001, TC-NFR-LAT-001 — `docs/srs/testing-strategy.md` §5.
- NimBLE-Arduino README for exact library id, `BLEHIDDevice` helper API, and security constant names — verify at install time.
