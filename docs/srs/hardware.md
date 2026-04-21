# AirGlove Hardware Specification

Scope: Phase I MVP (pair, pointer XY, left/right click). Phase II scroll and
Phase III deployment concerns are only referenced where they influence Phase I
hardware decisions. Cross-reference: `../../report/chapters/system_design.tex`,
`../../report/chapters/requirements.tex`.

## 1. Bill of Materials

| Item | Part | Qty | Role | Notes |
| --- | --- | --- | --- | --- |
| MCU board | ESP32 DevKit-C (WROOM-32) | 1 | Main controller, BLE-HID mouse, IMU host, touch-pad host | Dual-core 240 MHz, on-board USB-UART for flashing and Phase I power |
| IMU | MPU6050 breakout (GY-521 or equivalent) | 1 | 6-axis inertial sensing (accel + gyro) | I2C addr 0x68, 3.3 V, 400 kHz; Madgwick fusion runs on MCU |
| Fingertip pad - thumb | Copper tape or conductive fabric, ~10 x 15 mm | 1 | Common return electrode for capacitive chord detection | Routed to T0 (GPIO4) |
| Fingertip pad - index | Copper tape or conductive fabric, ~10 x 15 mm | 1 | Left-click electrode (touches thumb to complete chord) | Routed to T2 (GPIO2) |
| Fingertip pad - middle | Copper tape or conductive fabric, ~10 x 15 mm | 1 | Right-click electrode | Routed to T3 (GPIO15) |
| Fingertip pad - ring | Copper tape or conductive fabric, ~10 x 15 mm | 1 | Reserved - scroll in Phase II | Routed to T4 (GPIO13); unused in Phase I firmware |
| Hookup wire | 30 AWG silicone-insulated | ~2 m | Flexible routing from MCU pocket to fingertips and sensor pocket | Stranded, silicone jacket survives flex at knuckles |
| Battery | LiPo 3.7 V, 500-1000 mAh, single cell | 1 | Untethered operation | Protection circuit assumed; capacity finalised in Phase III (see Section 7) |
| Charger board | TP4056 USB-C module with protection IC (DW01 + FS8205) | 1 | LiPo charging, over-discharge and over-current protection | Verify that the variant includes the protection pair, not a bare TP4056 |
| Power switch | SPST slide switch, 1 A | 1 | Battery disconnect between charger output and MCU rail | Mounted near wrist for easy reach |
| Glove substrate | Thin knit or lycra glove (right-hand, user sized) | 1 | Wearable carrier for electrodes, wiring, MCU pocket, battery pocket | Breathable, non-conductive |
| MCU pocket | Fabric pouch sewn to back of hand | 1 | Mechanical anchoring and strain relief for ESP32 DevKit-C | Soft-sided to avoid pressure points |
| Battery pocket | Fabric pouch at wrist | 1 | Holds LiPo and charger board | Opens for charging cable access |

Conductive-pad material note: copper tape is cheaper and easier to iterate with
during Phase I; conductive fabric is preferred for the final wearable because it
flexes without cracking. Both are compatible with the ESP32 native capacitive
touch block with no code changes.

## 2. ESP32 Pinout

All GPIOs are 3.3 V logic. Touch channel numbers T0/T2/T3/T4 are ESP32 Touch
Sensor peripheral channels, not ordinal GPIO numbers; the physical GPIO mapping
is fixed by silicon.

| GPIO | Function | Connected to | Notes |
| --- | --- | --- | --- |
| GPIO4 | Touch T0 | Thumb pad (common return electrode) | Thumb is treated as the single roaming electrode that completes a chord against any of the other fingertip pads |
| GPIO2 | Touch T2 | Index fingertip pad | Left-click chord (thumb + index) |
| GPIO15 | Touch T3 | Middle fingertip pad | Right-click chord (thumb + middle) |
| GPIO13 | Touch T4 | Ring fingertip pad | Phase II scroll; pad installed, firmware reads reserved |
| GPIO21 | I2C SDA | MPU6050 SDA | Shared I2C bus, 400 kHz, 3.3 V |
| GPIO22 | I2C SCL | MPU6050 SCL | Shared I2C bus, 400 kHz, 3.3 V |
| 3V3 | 3.3 V rail out | MPU6050 VCC, pad substrate reference if needed | On-board LDO; do not source more than MCU rating |
| GND | Ground | MPU6050 GND, LiPo negative through switch, charger GND | Single-point star preferred at MCU pocket |
| EN | MCU reset | Pull-up on DevKit; reset button on-board | Not externally wired in Phase I |
| VIN (5 V USB) | Phase I power input | USB-C on DevKit-C | Phase I MVP is USB-powered; LiPo path added during Phase III power epic |
| USB D+ / D- | Programming and logs | On-board CP2102 | Not broken out to glove wiring |
| GPIO0 | Boot strap | On-board button | Keep floating after boot; do not wire to glove |
| GPIO5, 18, 19, 23, 25-27, 32, 33 | Reserved | Not connected | Held for Phase II/III expansion (haptics, LED indicator, extra sensors) |

Touch wiring model (confirmed, do NOT change):
- Thumb (T0) is the common return. The thumb pad is one of the four touch
  channels the ESP32 samples, and a "click" is detected when the thumb pad
  reading and the index (or middle) pad reading both drop together because
  skin on skin couples the two electrodes.
- Alternative wiring: "thumb tied to a common VCC line, fingertip electrodes
  wired to digital GPIOs read via `digitalRead`." This approach is rejected for
  Phase I. Reasons: it adds exposed DC voltage on the skin-facing conductor,
  gives no debounce telemetry, and does not reuse the ESP32 touch peripheral
  noise filtering. The touch-peripheral approach is what the firmware assumes.

## 3. Wiring Diagram

ASCII schematic (signal paths only; power return lines omitted for clarity
where they run alongside the signals):

```
                +---------------------------+
                |        ESP32 DevKit-C     |
                |                           |
   USB-C 5V --> | VIN                       |
                |                       3V3 | ---+------------------+
                |                       GND | ---|--+---------------|---+
                |                           |    |  |               |   |
                |                    GPIO21 | -- SDA ------+        |   |
                |                    GPIO22 | -- SCL ----+ |        |   |
                |                           |            | |        |   |
                |  Touch T0  (GPIO4)        | -----------|-|---- Thumb pad
                |  Touch T2  (GPIO2)        | -----------|-|---- Index pad
                |  Touch T3  (GPIO15)       | -----------|-|---- Middle pad
                |  Touch T4  (GPIO13)       | -----------|-|---- Ring pad  (reserved)
                |                           |            | |
                +---------------------------+            | |
                                                         | |
                                            +------------v-v------+
                                            |      MPU6050         |
                                            |  VCC  GND  SCL  SDA  |
                                            +----^----^----^----^--+
                                                 |    |    |    |
                                                3V3  GND  SCL  SDA
```

Reference figure for the layered architecture that this wiring implements:
`../../report/img/architectural_final.drawio.png` (chapter 6, system design).
A machine-readable source (drawio) is not yet committed; if/when it is added it
should live beside the PNG.

Notes on the diagram:
- The MPU6050 breakout's built-in pull-ups on SDA/SCL are used; no external
  pull-ups are added.
- Touch pads are single-ended: each fingertip pad is one wire from the GPIO to
  the conductive patch. There is no intentional ground electrode on the skin.
- All four touch wires are twisted together on the run from the MCU pocket to
  the fingertips to minimise common-mode pickup; they separate only at the
  last few centimetres near each fingertip.

## 4. Physical Layout on the Glove

Target hand: right hand (prototype). Left-hand mirror is a Phase III variant.

ASCII hand sketch (palm-down view, fingertips at the top):

```
                 Ring   Middle  Index  Thumb
                 [T4]    [T3]   [T2]   [T0]
                  |       |      |      |
                  +---+   +--+   +--+   +--+
                      \     \      \      \
                       +-----+------+------+------+
                                 wires to MCU
                                      |
                                      v
                          +-----------------------+
                          |    MCU pocket         |
                          |    (back of hand)     |
                          |   ESP32 DevKit-C      |
                          |   + MPU6050 on I2C    |
                          +-----------+-----------+
                                      |
                                 wrist strap
                                      |
                          +-----------v-----------+
                          |    Battery pocket     |
                          |    LiPo + TP4056      |
                          |    + power switch     |
                          +-----------------------+
```

Placement rationale:
- MCU on the back of the hand, not the palm: keeps the palm free for natural
  hand closure and surface contact, and places the IMU as close as possible to
  the rotational axes the user perceives (the wrist and knuckles), which makes
  tilt-to-cursor mapping more predictable.
- Battery on the wrist, not the MCU pocket: moves the heaviest part of the
  assembly onto the forearm side of the wrist joint, reducing perceived glove
  weight during sustained pointing.
- Wires run along the back of each finger to each fingertip pad, not along the
  palm side, to avoid compression under typing/desk contact.
- The research SWOT (Chapter 3) flags ergonomics and exposed-conductor safety
  as weaknesses; fingertip pads must be covered by a thin fabric overlay so the
  user never touches bare copper, and wiring must be routed so that closing the
  fist does not stretch or pinch a conductor.

## 5. Power Budget (Phase I Estimate)

All numbers below are order-of-magnitude planning estimates from datasheets and
typical ESP32 BLE figures. They are not measured values. Measurement is part of
the Phase III power epic (E14).

| Subsystem | Typical current | Source of estimate |
| --- | --- | --- |
| ESP32 active (240 MHz, radio idle) | ~150 mA | ESP32 datasheet typical active current with WiFi off, BLE idle |
| ESP32 BLE advertising and HID notifications | ~80 mA average additional | Espressif BLE app notes, averaged over advertising + notify duty cycle |
| MPU6050 active (accel + gyro on) | ~4 mA | MPU6050 datasheet |
| Touch peripheral, LDO overhead, misc | a few mA | Negligible vs radio |
| Estimated total running average | ~160 mA | Sum above, rounded |

Session runtime estimate with a 500 mAh LiPo and no sleep states:

```
500 mAh / 160 mA  ~=  3.1 h
```

This is below the project-level power target (NFR-PWR-001: at least 4 h of
continuous session use). Gap closure is explicitly deferred to Phase III epic
E14 (power management), which will introduce BLE connection-interval tuning,
light sleep between IMU samples, and possibly a 1000 mAh cell. The number
above is an estimate and must be re-measured on real hardware before it is
quoted as a spec.

## 6. Safety and Assembly Notes

- Fingertip pads must not expose bare metal to the skin. Cover each pad with a
  thin layer of fabric (one knit-glove layer is enough) so the capacitive
  coupling still works but the user never contacts the conductor directly.
- Strain relief at the MCU pocket: every wire leaving the pocket is anchored
  (hot glue, small heatshrink sleeve, or a stitched fabric loop) so that glove
  flex does not pull on solder joints.
- The LiPo must use a protected charge path. TP4056 modules that carry the
  DW01 + FS8205 protection pair are acceptable; bare TP4056 breakouts without
  the protection IC are not. Verify by visual inspection of the chosen board
  before integration.
- Avoid pressure on the ESP32 crystal and the MPU6050 QFN: pad the MCU pocket
  on the hand-facing side so knuckles flexing against the pocket do not stress
  the passives.
- Route battery wires with enough slack that the LiPo can shift inside its
  pocket without tugging on the solder joints at the charger board.
- Never charge the LiPo while the glove is being worn; the charging path stays
  outside the user session in Phase I.

## 7. Open Hardware Questions

- Battery capacity final choice (500 mAh vs 750 mAh vs 1000 mAh) - resolve
  during Phase III power epic E14 after measured current draw on real
  hardware.
- Glove substrate material (knit cotton vs lycra vs technical fabric with
  integrated conductive thread) - no dedicated epic yet; open question to be
  resolved when a "wearable form" epic is created (likely alongside E14).
- Charging connector location (wrist pocket USB-C pass-through vs removable
  battery with external charging dock) - tied to E14 (power management);
  influences whether the user charges the glove while wearing it.
- Right-hand vs left-hand variant and mirrored pinout - out of scope; no epic
  assigned. Phase I prototype is right-hand only.
- Scroll input mechanism final choice (ring-finger chord vs explicit mode
  button) - Phase II scroll epic E10; hardware pad for ring finger is already
  installed in Phase I so no rework is expected.
