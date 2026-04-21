# AirGlove Testing Strategy

Scope: Phase I MVP test plan (FR-001-FR-004 fully covered). FR-005-FR-010 have
placeholder test cases tracked as deferred in the traceability matrix so that
the matrix stays complete end-to-end. Cross-reference:
`../../report/chapters/requirements.tex`,
`../../report/chapters/system_design.tex`, and the hardware spec at
`./hardware.md`.

## 1. Purpose

AirGlove is verified across three test layers. Native unit tests cover the
pure-logic `srv_*` libraries with PlatformIO's `env:native` Unity runner,
running on the developer's host without hardware. On-target unit tests cover
the `dd_*` hardware drivers and the BLE-HID driver with `env:esp32dev` Unity
runs uploaded over USB to a real DevKit-C. Hardware-in-the-loop acceptance
tests run manually against a fully assembled glove paired to a host OS, and
map one-to-one onto the FR and NFR statements in the SRS. This split keeps
the fast feedback loop on the laptop, keeps hardware-specific verification
honest, and separates "does the code work" from "does the glove work."

## 2. Test Pyramid

```
                               ^
                              / \
                             /HIL\                Layer 3 - HIL / acceptance
                            / 6-8 \              (manual, per release)
                           /  TCs  \
                          +---------+
                         /           \
                        / on-target   \           Layer 2 - on-target unit
                       /   Unity       \         (dd_mpu6050, dd_touch,
                      /   ~15-25 tests  \          dd_ble_hid; per PR that
                     +-------------------+         touches a driver)
                    /                     \
                   /    native Unity       \      Layer 1 - native unit
                  /      ~60-120 tests      \    (srv_fusion, srv_motion,
                 /    (srv_fusion,           \    srv_input; every PR, CI)
                /      srv_motion,            \
               /       srv_input)              \
              +---------------------------------+
```

The tall base is intentional: every tunable number in the fusion, motion, and
input layers exists behind a pure function, so the base can absorb the bulk of
the verification work and the higher layers can stay thin.

## 3. Layer 1 - Native Unit Tests

Runner: PlatformIO `env:native` + Unity. Entry point: `test/test_native/` in
each `srv_*` library (or a shared test runner at the project root that
aggregates them, depending on PlatformIO scaffold decisions made outside this
document). Run from the `air-glove/` project directory:

```
pio test -e native
```

Libraries and what they cover:

- `srv_fusion` (Madgwick/Mahony orientation filter, dead-reckoning state):
  - Static input: feed a constant accel vector equal to gravity along the
    Z axis and zero gyro; after the filter's configured convergence window
    (a fixed number of iterations), the output quaternion is within a
    tolerance (target: `|q - q_identity| < 1e-2`) of the identity.
  - Bias rejection: inject a small constant gyro bias and verify that the
    accel reference pulls the estimate back within tolerance after
    convergence.
  - Noise stability: add zero-mean Gaussian noise to accel and gyro inputs;
    the orientation estimate standard deviation stays below a configured
    bound and never diverges over a long-running sequence.
- `srv_motion` (orientation -> cursor delta mapping):
  - Dead zone: inputs whose magnitude is below the configured threshold
    produce exactly zero cursor delta.
  - Gain curve monotonic: for any two inputs `a < b` above the dead zone,
    the mapped magnitude satisfies `map(a) <= map(b)`.
  - Sign preservation: positive tilt in roll produces positive X delta,
    negative tilt produces negative X delta, and the same on pitch/Y; no
    axis swap is accidentally introduced.
  - Clutch gate: when the clutch state is "engaged", `srv_motion` outputs
    zero delta regardless of orientation input; when the clutch releases,
    no residual step is emitted in the first post-release tick.
- `srv_input` (touch chord -> HID click events):
  - Debounce: a single-sample glitch on any pad is filtered out; the
    sustained touch over the configured debounce window fires exactly one
    click event.
  - Chord detection: simultaneous thumb + index press fires one left-click;
    simultaneous thumb + middle fires one right-click; overlapping releases
    do not generate spurious second events.
  - Edge semantics: click event fires on press edge, not continuously while
    held; a second press after release generates a second event.

Target coverage for the `srv_*` libraries combined: >= 80 % line coverage as
reported by `gcovr` (or PlatformIO's coverage plugin, whichever is configured
in the scaffold step). Coverage below 80 % blocks the PR that introduces the
drop.

## 4. Layer 2 - On-Target Unit Tests

Runner: PlatformIO `env:esp32dev` + Unity. Tests upload over USB to a real
DevKit-C and stream results back over the serial monitor. Run from the
`air-glove/` project directory with the DevKit-C plugged in:

```
pio test -e esp32dev
```

Drivers and what they cover:

- `dd_mpu6050`:
  - `WHO_AM_I` register read returns `0x68` (confirms I2C is wired, pull-ups
    are present, and the address is correct).
  - Accelerometer reading with the board lying flat is within +/- 0.2 g of
    `(0, 0, 1 g)` in the configured range (+/- 2 g).
  - Gyroscope reading with the board stationary is within the datasheet
    zero-rate spec (roughly +/- 5 deg/s on a cold, uncalibrated part); the
    test asserts the magnitude is below that bound, not a specific value.
- `dd_touch`:
  - Baseline untouched reading on each of T0, T2, T3, T4 is above 40 raw
    units (the ESP32 touch peripheral returns "higher number = less
    capacitance to ground", so untouched is the high-reading state).
  - Touched reading drops at least 30 % below baseline; the test prompts
    the operator over serial to touch each pad in turn and asserts the
    observed delta.
  - The runtime threshold calibration routine (baseline sampling over a
    short window, then applying a fixed percentage margin) converges to a
    stable threshold over repeated runs; stability is asserted by running
    the routine twice and requiring the two thresholds to differ by less
    than a configured epsilon.
- `dd_ble_hid`:
  - Device advertises with the expected name ("AirGlove") after init.
  - Accepts a paired connection from a known test host (operator confirms
    over serial).
  - Sends a zero HID mouse report (no buttons, zero delta) without error
    return codes; the test does not attempt to verify host-side receipt at
    this layer (that is covered in Layer 3).

## 5. Layer 3 - HIL / Acceptance Tests

Executed manually on a fully assembled glove paired to a host. Each test case
maps directly to a requirement. Operator writes the pass/fail and the
measured value (where applicable) in the release checklist.

| ID | Requirement | Procedure | Pass criterion |
| --- | --- | --- | --- |
| TC-FR01-01 | FR-001 (device init and HID startup) | Power the glove on from a cold state. Watch the MCU status (serial boot log or on-board LED). Open the host's Bluetooth settings. | Within 5 s of power-on, host's Bluetooth scanner lists a device advertising as "AirGlove"; serial log shows IMU init OK and BLE advertising started. |
| TC-FR02-01 | FR-002 (pair + auto-reconnect) | Pair from Windows 10/11 the first time. Disconnect the glove by powering it off, wait 10 s, power it on again. | Host reconnects automatically within 5 s of power-on with no pairing prompt. |
| TC-FR03-01 | FR-003 (cursor motion) | With cursor at screen centre, tilt the glove 30 deg right from neutral and hold. Then return to neutral and hold still for 10 s. | Cursor moves to the right at a stable rate while tilted, stops when returned to neutral, and drifts less than 2 px/s while held still (satisfies NFR-STAB-001). |
| TC-FR04-01 | FR-004 (left click) | With cursor over a clickable target, touch thumb to index pad once. Repeat 10 times with brief holds. | Each touch produces exactly one left-click event on the host; no double-fires; click still works while the cursor is in slow motion. |
| TC-FR04-02 | FR-004 (right click) | With cursor over a context-menu target, touch thumb to middle pad once. Repeat 10 times. | Each touch produces exactly one right-click event; context menu opens; no left-click leakage. |
| TC-NFR-LAT-001 | NFR-LAT-001 (latency) | Run the host-side timestamping script described in Section 6; move the glove in a sharp step and record the IMU-event to host-event delta over 100 steps. | p95 latency <= 100 ms from IMU sample to host input event. |
| TC-NFR-HID-001 | NFR-HID-001 (cross-platform HID) | Pair the same unmodified glove to a Windows 10+ machine, an Ubuntu 22.04+ machine, and a macOS 12+ machine in sequence. | Each host recognises the device as a standard Bluetooth mouse with no driver install, and cursor motion plus left/right click work on each. |
| TC-NFR-STAB-001 | NFR-STAB-001 (cursor stability) | Hold the glove stationary on a flat surface for 60 s with BLE connected. | Accumulated cursor drift remains below 2 px/s average and no spurious click events are reported. |
| TC-NFR-PWR-001 | NFR-PWR-001 (session endurance) | Charge the LiPo to full, disconnect USB, exercise the cursor and click at a realistic duty cycle until the device stops advertising. Record wall-clock time. | Recorded session time documented; gap against the 4 h target logged under Phase III epic E14 (this TC is informational in Phase I - not a pass/fail gate). |

## 6. Latency Measurement Method

Procedure (manual test, not automated in CI):

- Firmware side: toggle a dedicated GPIO high for a few milliseconds each
  time an IMU sample crosses a configured step threshold. Wire that GPIO to
  an LED (or a photodiode if a camera is not available).
- Host side (Linux): a small Python script reads `/dev/input/event*` for the
  paired HID device, captures the monotonic timestamp of each mouse-motion
  event, and logs them to a CSV.
- Host side (Windows): a small C#/Rust script uses the Raw Input API
  (`RAWINPUT`) and the high-resolution performance counter, and logs mouse
  event timestamps to a CSV.
- Cross-reference: a phone or high-speed camera records both the LED pulse
  and the screen cursor motion at 120-240 fps; the operator reads back the
  frame number of the LED flash and the frame number of the first cursor
  displacement, converts to milliseconds, and compares against the host
  CSV. The camera reading is the authoritative end-to-end number; the CSV
  gives a faster proxy for quick iteration.
- The p95 of 100 sharp step motions is the reported number. Single outliers
  above 100 ms are recorded but do not count against pass/fail; the p95
  figure is what gates the release.

This is explicitly a manual test. Automating it would require a dedicated
rig (photodiode + microcontroller timestamping + scripted test host) that is
out of scope for Phase I.

## 7. Test Case ID Convention

IDs take the form `TC-<REQ_ID>-<seq>`, where `<REQ_ID>` is the functional or
non-functional requirement ID (`FR01`-`FR10`, or `NFR-<family>-NNN`), and
`<seq>` is a zero-padded two-digit sequence number starting at `01` inside
each requirement. Every FR and every NFR has at least one TC; requirements
that need multiple angles of verification (for example FR-004 covers both left
and right click) have one TC per angle.

NFR family codes used in this document (aligned with the brief and with the
non-functional narrative of `requirements.tex`):

- `LAT` - latency / responsiveness
- `STAB` - cursor stability and drift
- `HID` - host-side HID compatibility
- `PWR` - power / session endurance

Additional NFR families (usability, ergonomics, safety, maintainability,
portability, wireless reliability) exist in the SRS narrative but do not
have numeric targets yet and so do not have automated test cases in
Phase I; they are carried as deferred in the traceability matrix.

## 8. Traceability Matrix

Phase I requirements are fully covered. Phase II and III requirements are
listed so the matrix is complete; their test cases are deferred until the
corresponding epic starts and are tracked in the backlog.

| Requirement ID | Requirement (short) | Test case IDs | Owning epic | Status |
| --- | --- | --- | --- | --- |
| FR-001 | Device init and HID startup | TC-FR01-01 | E05, E09 | Phase I |
| FR-002 | Pairing and auto-reconnect | TC-FR02-01 | E05, E09 | Phase I |
| FR-003 | Cursor motion from IMU | TC-FR03-01; plus native tests on `srv_fusion` and `srv_motion` | E03, E06, E07, E09 | Phase I |
| FR-004 | Finger-contact click | TC-FR04-01, TC-FR04-02; plus native tests on `srv_input` and on-target tests on `dd_touch` | E04, E08, E09 | Phase I |
| FR-005 | Scroll interaction | (deferred - TC-FR05-* defined when epic starts) | E10 | Phase II (backlog) |
| FR-006 | Clutch and hand repositioning | (deferred - TC-FR06-*; native tests on clutch gate in `srv_motion` land in Phase I) | E10 | Phase II (backlog) |
| FR-007 | Adjustable sensitivity profiles | (deferred - TC-FR07-*) | E11 | Phase II (backlog) |
| FR-008 | User calibration workflow | (deferred - TC-FR08-*) | E12 | Phase II (backlog) |
| FR-009 | Fault-safe runtime behavior | (deferred - TC-FR09-*) | E13 | Phase III (backlog) |
| FR-010 | HID cross-platform compatibility | TC-NFR-HID-001 (primary); also covered per-host by running TC-FR01-01 on each OS | E05 (Phase I), E13 (Phase III hardening) | Phase I verification, hardening in Phase III |
| NFR-LAT-001 | End-to-end latency p95 <= 100 ms | TC-NFR-LAT-001 | E05, E06, E07, E09 | Phase I |
| NFR-STAB-001 | Drift < 2 px/s at rest | TC-NFR-STAB-001, TC-FR03-01 | E06, E07 | Phase I |
| NFR-HID-001 | Works on Windows 10+, Ubuntu 22.04+, macOS 12+ with no driver install | TC-NFR-HID-001 | E05 | Phase I |
| NFR-ERG-001 | Total glove mass <= 80 g; no exposed conductors | Bench weigh + visual inspection (manual, tracked in `docs/plans/09-integration-and-bringup.md`) | E01 (hardware notes), E04 | Phase I |
| NFR-MOD-001 | Layer isolation (srv_*/app_* never include Arduino/ESP-IDF) | Enforced by `pio run -e native` building all `srv_*` and `app_*` libs | E01, E02 | Phase I |
| NFR-PWR-001 | >= 4 h continuous session | TC-NFR-PWR-001 (informational in Phase I) | E14 | Phase III (backlog) |
| Additional narrative NFRs (usability, safety, maintainability, portability, wireless reliability) | Narrative in `report/chapters/requirements.tex` | Deferred - no numeric targets yet; re-evaluate once a specific epic owns them | (no epic assigned) | Phase III |

## 9. CI Guidance

- Layer 1 (native unit tests) runs in CI on every pull request. A GitHub
  Actions workflow on an `ubuntu-latest` runner installs PlatformIO
  (`pip install platformio`) and invokes `pio test -e native` from the
  `air-glove/` project directory. The run is fast (single-digit seconds
  range once the cache is warm) and gates merges.
- Layer 2 (on-target unit tests) is not run in CI because it needs a
  physical DevKit-C plugged in. It is run locally by the engineer before
  a release candidate is cut, and the run transcript is attached to the
  release checklist.
- Layer 3 (HIL / acceptance) is entirely manual, run against a real glove
  and a real host, and its results populate Section 5's table for each
  release candidate.
- A future automation epic can migrate Layer 2 to a self-hosted runner with
  a DevKit-C attached; that is out of scope for Phase I.
