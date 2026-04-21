# Plan 03 — dd_touch (capacitive touch device driver)

- **Epic:** [E04](../../.claude/memory/epics/E04-touch-driver.md)
- **Goal:** Initialise the ESP32 native touch peripheral on T0/T2/T3/T4, auto-calibrate per-pad baselines at boot, and expose raw readings plus a `touched_mask` under one timestamp via `dd_touch_read()`.
- **Preconditions:** Plan 01 (scaffolding) has created `air-glove/lib/dd_touch/`. E02 has defined `touch_sample_t`, `touch_pad_id_t`, and `TOUCH_PAD_COUNT = 4`. Pads are wired per `hardware.md` §2 (T0→GPIO4 thumb, T2→GPIO2 index, T3→GPIO15 middle, T4→GPIO13 ring).

## Files to create

| Path (under `air-glove/lib/dd_touch/`) | Role |
|---|---|
| `include/dd_touch.h` | Public header (init + read). |
| `src/dd_touch.cpp` | Arduino-wrapper implementation using `touchRead()`. |
| `library.json` | PlatformIO lib manifest; `env:esp32dev` only. |
| `test/test_dd_touch/test_main.cpp` | On-target Unity test (baseline stability + touch-drop). |

## Step-by-step

1. In `dd_touch.cpp`, declare a file-scope `static const uint8_t kGpio[TOUCH_PAD_COUNT] = { 4, 2, 15, 13 };` — indices align with the `touch_pad_id_t` enum (THUMB=0, INDEX=1, MIDDLE=2, RING=3).
2. File-scope state: `static uint16_t baseline[4];`, `static uint16_t threshold[4];`, `static const float kThreshRatio = 0.7f;`, `static const float kEmaAlpha = 0.01f;`, `static bool initialized = false;`.
3. In `dd_touch_init()`:
   1. For each pad `i`, call `touchRead(kGpio[i])` once to prime the peripheral (first reading is often zero).
   2. `delay(10);` to let the ESP32 touch controller settle.
   3. For each pad `i`, loop 50 times: `sum += touchRead(kGpio[i]); delayMicroseconds(200);`. Store `baseline[i] = (uint16_t)(sum / 50)`.
   4. Compute `threshold[i] = (uint16_t)(baseline[i] * kThreshRatio)` — a pad is "touched" when its raw reading drops below this value (ESP32 touch reads are inversely proportional to capacitance).
   5. Set `initialized = true`; return `AG_OK`.
4. In `dd_touch_read(touch_sample_t *out)`:
   1. If `out == nullptr || !initialized`, return `AG_ERR_ARG` (or `AG_ERR_INIT`).
   2. For each pad `i`, `out->raw[i] = (uint16_t) touchRead(kGpio[i]);`.
   3. Build `touched_mask`: bit `i` is set when `out->raw[i] < threshold[i]`.
   4. For each untouched pad (`bit i clear`), update baseline via EMA: `baseline[i] = (uint16_t)((1.0f - kEmaAlpha) * baseline[i] + kEmaAlpha * out->raw[i]);` and refresh `threshold[i] = baseline[i] * kThreshRatio`. Do NOT update when touched — otherwise a held touch slowly redefines "idle" as "low".
   5. `out->t_us = (uint64_t) esp_timer_get_time();`
   6. `out->touched_mask = mask;`
   7. Return `AG_OK`.
5. Note in a file-top comment that `touchRead()` is the Arduino wrapper and a future swap to `touch_pad_read_raw_data()` from ESP-IDF (driver/touch_sensor.h) is planned (faster, interrupt-capable, non-blocking). Track this swap in E14 (power) as a low-priority follow-up.

## Public header (target signature)

```c
// lib/dd_touch/include/dd_touch.h
#pragma once
#include "ag_types.h"

// Initialise the ESP32 touch peripheral for channels T0/T2/T3/T4 and
// auto-calibrate each pad baseline (50-sample mean) with threshold =
// baseline * 0.7. Returns AG_OK or AG_ERR_INIT. Fingers must be off-pad
// at boot; if a pad is touched during init its baseline will be wrong
// until the slow EMA reclaims it. Thread-safety: call once from one task.
ag_result_t dd_touch_init(void);

// Read all four pads, update running baseline for untouched pads via EMA
// (alpha = 0.01), fill *out with raw values, touched_mask (bit i = pad i),
// and an esp_timer_get_time() timestamp. Thread-safety: single-caller.
ag_result_t dd_touch_read(touch_sample_t *out);
```

## Internal design notes

- `touchRead()` is blocking (~1 ms per pad on default settings → ~4 ms per full read). At a 10 ms `t_touch` tick this fits. If bring-up measurements show it overrunning, swap to `touch_pad_read_raw_data()` + `touch_pad_filter_start()` from ESP-IDF for background sampling.
- Threshold is a simple `baseline * 0.7` (a 30 % drop). The E04 epic specifies `baseline − baseline * 0.3` which is algebraically identical; we keep the multiplicative form for clarity.
- EMA alpha = 0.01 at 100 Hz → time constant ~1 s. This is fast enough to track skin-moisture/temperature drift but slow enough that a single brushed touch cannot drag the baseline down.
- State layout: `baseline[4]` + `threshold[4]` = 16 bytes; plus `initialized` flag. No heap.
- Phase I reads the thumb pad but does NOT use it for button mapping. The thumb is the common return electrode: per `hardware.md` §2, a click is detected when BOTH thumb and a finger pad drop together. E09 button-mapping uses only `TOUCH_PAD_INDEX` and `TOUCH_PAD_MIDDLE` events and leaves the thumb event as informational.

## Verification

On-target Unity test (`test/test_dd_touch/test_main.cpp`), runs under `env:esp32dev`:

- `test_init_returns_ok` — first call to `dd_touch_init()` returns `AG_OK`.
- `test_baseline_stable_1s` — call `dd_touch_read()` 100 times at 10 ms spacing without touching; for each pad `i`, stddev of `raw[i]` / mean < 0.05 (5 % variance, per E04 acceptance).
- `test_timestamp_monotonic` — successive `out->t_us` values strictly increase and are identical across pads in the same call.
- `test_touch_drops_reading` — interactive: `printf("touch INDEX pad within 3 s\n")`, read every 50 ms; assert that at some point `raw[INDEX] < baseline[INDEX] * 0.7f` and `touched_mask & (1 << TOUCH_PAD_INDEX)` is set. Repeat for MIDDLE and RING.
- `test_calibration_repeatable` — run init twice 500 ms apart; for each pad, `abs(baseline_run1 - baseline_run2) < baseline_run1 * 0.1`.

Manual bring-up probes:

- `printf("baseline T0=%u T2=%u T3=%u T4=%u\n", baseline[0], ...)` — typical idle values are 50-80 raw units on a DevKit-C with short wires; much lower values indicate a shorted pad or wire-to-ground leak.
- Oscilloscope on any touch pin: the peripheral drives a sampling pulse; the pulse width shortens under touch. Useful when raw values look wrong.

PlatformIO command:

```
pio test -e esp32dev -f test_dd_touch
```

## Rollback / risk

- Smallest revert: stub `dd_touch_read()` to fill `out->raw[i] = 50; out->touched_mask = 0;` — the rest of the stack still runs and just reports "no clicks".
- Known risks:
  - Baseline captured during a touch at boot → threshold permanently wrong for that pad. Early warning: `touched_mask` is always zero for one pad. Mitigation: the slow EMA eventually reclaims; user workaround is to reset with fingers off.
  - Noise from the I2C bus on adjacent GPIOs. Not expected at 400 kHz over short runs, but if pad readings show periodic dips at the 10 ms IMU tick, twist and separate the touch wires from the I2C pair.
  - Long unshielded fingertip wires pick up 50/60 Hz mains hum. Mitigation from `hardware.md`: twisted-pair routing from MCU pocket to fingertips.

## References

- architecture.md §3, §4.1 (task `t_touch`), §6.2 (touch path data flow)
- decisions.md ADR-003 (ESP32 native capacitive touch)
- epic E04 (scope, acceptance criteria)
- hardware.md §2 (pin allocation T0/T2/T3/T4), §6 (pad safety)
- Arduino-ESP32 `touchRead()` reference — `esp-idf/driver/touch_sensor.h` for the future native swap
