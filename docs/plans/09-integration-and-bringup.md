# Plan 09 — Integration & Bring-up Runbook

- **Epic:** E09 (see `.claude/memory/epics/E09-application-controller.md`) — this is the HIL half that complements Plan 08.
- **Goal:** Serve as the authoritative bring-up runbook and acceptance script for the Phase I MVP. Every Phase I FR and NFR test case is run from this document against a fully assembled glove.
- **Preconditions:**
  - Plans 01–08 complete — code compiles, all `dd_*` and `srv_*` unit tests pass.
  - A fully assembled glove matching `docs/srs/hardware.md` BOM and pinout.
  - One Windows 10/11 host, one Ubuntu 22.04+ host, one macOS 12+ host available (see §9 for partial-coverage handling).
  - USB-C cable, serial monitor at 115200 baud.

## 1. Pre-flight

Inspect the physical build against `docs/srs/hardware.md` Sections 1–3.

| # | Check | Pass criterion |
|---|-------|----------------|
| 1 | BOM complete | ESP32 DevKit-C, MPU6050 breakout, 4 fingertip pads, hookup wire, LiPo + TP4056, switch — all present and mounted per §4 of hardware.md. |
| 2 | Pinout | GPIO4/T0 thumb, GPIO2/T2 index, GPIO15/T3 middle, GPIO13/T4 ring, GPIO21 SDA, GPIO22 SCL. Verified by continuity meter. |
| 3 | MPU6050 power | 3V3 on VCC pin, GND solid. Power LED on the breakout lights when USB is connected. |
| 4 | No shorts | Resistance from 3V3 to GND > 10 kΩ with MCU removed; > 1 kΩ with MCU seated. |
| 5 | Strain relief | Every wire leaving the MCU pocket is anchored per hardware.md §6. |
| 6 | Pads covered | No bare copper touches skin; thin fabric overlay on every fingertip pad. |
| 7 | USB path | USB-C cable connects; host enumerates the CP2102 serial port. |

If any row fails, stop. Do not proceed to flashing.

## 2. Flash + boot smoke

From `air-glove/`:

```
pio run -e esp32dev -t upload
pio device monitor -b 115200
```

Expected boot log (exact `printf` strings emitted from `app_controller_start()` and the `dd_*_init()` calls — names per Plan 08):

```
app_controller: starting Phase I
dd_mpu6050 WHO_AM_I=0x68 OK
dd_touch baseline[THUMB]=<n> [INDEX]=<n> [MIDDLE]=<n> [RING]=<n>
dd_ble_hid advertising: AirGlove
app_controller: state=PAIRING
app_controller: heartbeat t=<s> tasks=6 hwm[imu]=<n> hwm[fusion]=<n> hwm[touch]=<n> hwm[motion]=<n> hwm[app]=<n> hwm[ble]=<n>
```

Pass criteria:
- No panic / reboot in the first 30 s.
- Every `dd_*_init` line prints `OK` or a meaningful numeric baseline.
- Heartbeat line appears at the expected cadence (see app_controller config; typical 1–2 s period).

## 3. Driver sanity probes (in order)

Run these before pairing — they confirm the stack is healthy layer by layer.

1. **IMU probe.**
   - Expected line: `dd_mpu6050 WHO_AM_I=0x68 OK`.
   - Secondary line (periodic): `dd_mpu6050 accel_mag=<g>` with the board flat should be within 0.2 of 1.00.
   - Fail → §11 "MPU6050 I²C NACK".
2. **Touch probe.**
   - Expected: `dd_touch baseline[THUMB]=... [INDEX]=... [MIDDLE]=... [RING]=...` with all four values in the hundreds range (ESP32 touch reads higher = less capacitance; datasheet-dependent). Values differ per pad; sanity range is 40–1000.
   - Pinch thumb-to-index once; expect the INDEX reading to drop by ≥ 30 % momentarily.
   - Fail → §11 "Touch pad stuck / no delta".
3. **BLE advertising probe.**
   - Expected: `dd_ble_hid advertising: AirGlove`.
   - Confirm with a host-side BLE scanner (e.g. `bluetoothctl scan on` on Linux, "nRF Connect" app, or Windows BT Settings). The device must appear as **AirGlove** with Appearance = HID Mouse (0x03C2).
   - Fail → §11 "BLE not appearing".

## 4. Pairing (TC-FR01-01, TC-FR02-01)

### Windows 10 / 11

1. Settings → Bluetooth & devices → Add device → Bluetooth.
2. Select **AirGlove** from the list. Pairing is just-works; Windows should not prompt for a PIN.
3. Wait until Windows reports "Your device is ready to go".
4. Verify: device listed under "Mouse, keyboard, & pen" with icon = mouse; Device Properties shows HID class.
5. **Auto-reconnect test (TC-FR02-01):** power the glove off via the slide switch, wait 10 s, power back on. Host must reconnect within 5 s with no dialog.

### Ubuntu 22.04+

1. `bluetoothctl` → `scan on` → wait for `AirGlove` → `pair <MAC>` → `trust <MAC>` → `connect <MAC>`.
2. No PIN prompt expected.
3. Verify: `/dev/input/by-id/` contains an entry with `bluetooth-mouse` in the name pointing to a new `event*` node.
4. `xinput list` (X11) or `libinput list-devices` (Wayland) shows the device as a pointer.
5. Auto-reconnect: power-cycle the glove; `bluetoothctl` shows `Connected: yes` within 5 s.

### macOS 12+

1. System Settings → Bluetooth → wait for **AirGlove** to appear → click Connect.
2. No PIN prompt expected.
3. Verify: device listed as a mouse; "About This Mac" → System Report → Bluetooth shows HID class.
4. Auto-reconnect: power-cycle the glove; macOS should reconnect in under 5 s.

All three OSes must show the device identically — no driver install, no warning dialog.

## 5. Cursor motion smoke (TC-FR03-01)

1. Centre the cursor on a neutral area of the screen.
2. Hold the glove in neutral pose; confirm cursor is stationary.
3. Tilt the glove 30° to the right → cursor moves right at a stable rate.
4. Return to neutral → cursor stops.
5. Tilt 30° forward → cursor moves up; backward → cursor moves down. Confirm axis mapping matches `srv_motion` conventions (see Plan 06).
6. **Drift at rest:** hold neutral for 10 s. Cursor must drift less than 2 px/s on average (NFR-STAB-001).

If axis is inverted → flip sign in `app_config` (motion gain), not in the driver.

## 6. Click smoke (TC-FR04-01, TC-FR04-02)

**Left click (thumb + index):**
1. Position cursor over a desktop shortcut.
2. Pinch thumb-to-index briefly (~150 ms contact), release.
3. Expected: exactly one left-click fires; shortcut highlights or launches (double-click behaviour depends on OS).
4. Repeat 10 times in a row; count host click events. Must be exactly 10.

**Right click (thumb + middle):**
1. Position cursor over empty desktop.
2. Pinch thumb-to-middle briefly, release.
3. Expected: context menu opens.
4. Close menu, repeat 10 times; must produce 10 context-menu openings.

**Cross-talk check:** thumb-to-index must NOT fire a right-click, and thumb-to-middle must NOT fire a left-click. Ring pad (T4) is reserved Phase II and must not produce any event.

## 7. Latency measurement (TC-NFR-LAT-001)

### Method (Linux host, preferred because `evdev` is script-friendly)

Device side:
- Reuse the debug GPIO toggle already wired in `app_controller` (toggle GPIO25 high on every IMU sample crossing a step threshold).
- Connect that GPIO to the external trigger input of a logic analyser or to a photodiode circuit.

Host side — `latency_probe.py` (place alongside Plan 09):

```python
# Requires: sudo apt install python3-evdev
import evdev, time, sys
dev_path = sys.argv[1]  # e.g. /dev/input/by-id/<...bluetooth-mouse>-event-mouse
dev = evdev.InputDevice(dev_path)
print(f"listening: {dev.name}")
samples = []
for event in dev.read_loop():
    if event.type == evdev.ecodes.EV_REL:
        samples.append(time.time_ns())
        if len(samples) % 100 == 0:
            print(f"{len(samples)} rel events")
        if len(samples) >= 1000:
            break
with open("host_ts.csv", "w") as f:
    for ns in samples:
        f.write(f"{ns}\n")
print("wrote host_ts.csv")
```

Measurement loop:
1. Start `sudo python3 latency_probe.py /dev/input/by-id/<mouse-node>` on the host.
2. Start the logic analyser capturing the device GPIO edge with host-referenced timestamps (use a host-side trigger relay or an analyser that timestamps against the host clock).
3. Generate sharp tilt steps at ~1 Hz for ~1000 events.
4. Offline: for each GPIO rising edge, find the nearest subsequent EV_REL timestamp in `host_ts.csv`; compute delta.
5. Compute p50 and p95 over the 1000-sample set.

**Pass criterion:** p95 of 1000 samples ≤ 100 ms. Single-shot outliers do not fail the test; the p95 figure is what gates.

### Windows fallback

If no Linux host is available, follow the camera-based procedure in `docs/srs/testing-strategy.md` §6 (phone at 120–240 fps capturing LED pulse and cursor displacement). Record in the TC-NFR-LAT-001 row which method was used.

## 8. Stability / soak (TC-NFR-STAB-001 + TC-FR03-01 drift)

1. Lay the glove flat and stationary on a desk, BLE connected.
2. Run a host-side logger that samples the cursor position every 250 ms for 10 minutes:
   - Linux: `while true; do xdotool getmouselocation; sleep 0.25; done > soak.log`
   - Windows: small PowerShell script polling `[System.Windows.Forms.Cursor]::Position` at 4 Hz.
3. Compute median drift in 10 s windows over the 10-minute run.

Pass criteria:
- Median drift < 2 px/s across all 10 s windows.
- No spurious click events observed on the host during the 10 minutes.
- Serial heartbeat still prints throughout; no reboots.
- Final heartbeat stack high-water marks < 75 % on every task (per E09 acceptance).

## 9. Cross-OS matrix (TC-NFR-HID-001)

Fill in after running §4 and §§5–6 on each host.

| OS | Version | Pair OK | Recognised as mouse | Motion OK | L click | R click | Auto-reconnect | Notes |
|----|---------|---------|---------------------|-----------|---------|---------|----------------|-------|
| Windows | 10 / 11 | | | | | | | |
| Ubuntu | 22.04 / 24.04 | | | | | | | |
| macOS | 12 / 13 / 14 | | | | | | | |

Rules:
- A host that is unavailable is logged "Not Verified" — never blank and never "Pass".
- Any "Fail" cell blocks the release.

## 10. Exit criteria

The MVP is shippable as Phase I only when **every** row below is green.

| Criterion | Source |
|-----------|--------|
| TC-FR01-01 pass on all three OSes | §4 |
| TC-FR02-01 pass on Windows (primary host) | §4 |
| TC-FR03-01 pass (direction + drift) | §5 |
| TC-FR04-01 and TC-FR04-02 pass (10/10 clicks each) | §6 |
| TC-NFR-LAT-001 p95 ≤ 100 ms | §7 |
| TC-NFR-STAB-001 drift < 2 px/s median | §8 |
| TC-NFR-HID-001 all three OSes marked Pass | §9 |
| All heartbeat `hwm[*]` lines report < 75 % stack usage | §8 |
| No reboots or panics in 10 min soak | §8 |

## 11. Troubleshooting appendix

| Symptom | First probe | Likely cause | Fix |
|---------|-------------|--------------|-----|
| No `dd_ble_hid advertising` line | Serial log at boot | NimBLE init failure | Erase NVS (`pio run -e esp32dev -t erase`), reflash; check `setSecurityAuth` args in Plan 04. |
| Device advertises but host can't see it | Host-side BT scanner | Host BT radio off or stale cache | Toggle host Bluetooth off/on; remove stale "AirGlove" entry from host BT settings and retry. |
| Pairing prompts for a PIN | Host dialog text | IO cap mis-set; MITM accidentally enabled | In `dd_ble_hid.cpp` confirm `setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT)` and `setSecurityAuth(true,false,true)`. |
| macOS refuses to complete pairing | macOS BT log (`log stream --predicate 'subsystem == "com.apple.bluetooth"'`) | Secure-connections flag mismatch | Try `setSecurityAuth(true, false, false)` and re-test. |
| Host reconnect fails after reboot | `bluetoothctl` or Windows BT log | Bond evicted or NVS cleared | Re-pair; confirm `nvs_flash_erase` is not called in production boot path. |
| `dd_mpu6050 WHO_AM_I` returns wrong value or times out | Scope SDA/SCL lines | I²C NACK — missing pull-ups, address wrong, or wiring short | Confirm MPU6050 breakout pull-ups present; verify GPIO21/22 continuity; power-cycle. |
| Touch baseline = 0 or wildly unstable | Serial baseline line | Pad shorted to GND, or ADC floating | Remove pad, read again (should return a baseline); re-solder pad wire. |
| Touch pad "stuck" pressed | `dd_touch` raw readings in heartbeat | Moisture or baseline drift | Run `dd_touch` recalibration routine at boot; increase detection margin in `app_config`. |
| Cursor jumps / huge deltas | Heartbeat drop counter on `q_imu` | Missed fusion frames or fusion divergence | Check `q_imu` drop counter — if > 0 the producer is faster than consumer; increase `t_fusion` priority or check for a priority inversion. |
| Cursor drifts while held still | Drift measurement in §8 | Madgwick beta too low, or uncalibrated gyro bias | Tune `srv_fusion` beta (see Plan 05); add a bias-estimation warm-up. |
| Wrong click fires for pinch | §6 cross-talk check | Pad IDs swapped in wiring or `app_config` | Verify GPIO-to-pad mapping in `app_config` matches hardware.md §2. |
| Device reboots under stress | Serial log captures `Guru Meditation Error` | Stack overflow on a pinned task | Raise the guilty task's stack size in `app_config`; investigate if a lib is doing unbounded recursion or large local arrays. |

## References

- Epic E01 — `.claude/memory/epics/E01-project-foundation.md` (scaffolding / build).
- Epic E05 — `.claude/memory/epics/E05-ble-hid-driver.md` (BLE HID transport).
- Epic E09 — `.claude/memory/epics/E09-application-controller.md` (tasks, queues, heartbeat, acceptance).
- Testing strategy §5 (HIL tests) and §6 (latency method) — `docs/srs/testing-strategy.md`.
- Hardware spec §§2–5 (pinout, diagram, power) — `docs/srs/hardware.md`.
- Architecture §4 (task table, queue table, priorities) — `docs/srs/architecture.md`.
