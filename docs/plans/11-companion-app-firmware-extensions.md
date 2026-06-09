# Plan 11 — Companion App Firmware Extensions

Builds on `feat/web-app` (Patricia Moraru): `dd_ble_cfg`, `t_cfg`, the NimBLE companion service, and the React/Vite web client are assumed to exist. This plan **extends** that work — it does not replace it.

- **Goal:** Make the companion app actually configure the glove. Today the wire works, NVS works, telemetry works — but writing `sens_x` in the web UI does nothing to the cursor, and the schema is too thin to back the rest of the mock. Close the apply loop, grow the schema to v2 with per-finger thresholds and mappings, and let users assign actions to fingers and one chord modifier.
- **Preconditions:**
  - `feat/web-app` merged to `main` (or rebased on `f50cc64` first; see Plan 09 follow-up below).
  - The 90° MPU axis remap from `f50cc64` is in effect.
- **Non-goals (deferred):**
  - **Real IMU calibration.** The fake progress bar stays. Real dwell + motion-guard + bias storage is Plan 12.
  - GATT link encryption / pairing enforcement. The companion service stays open in v2; lock down with Plan 13.
  - OTA. Not in scope here either.
- **No-fallback rule.** Wire schema goes from v1 → v2 in one step. An old client connecting will see `version = 2` and refuse — that's intended. There is no v1↔v2 translation table.

---

## 11.1 — Close the apply loop

Single highest-value change in this plan. Currently `t_cfg_fn` polls `dd_ble_cfg_take_command()` but the config struct read from `dd_ble_cfg_get_config()` is never fed back into `srv_motion` / `srv_input` / `srv_fusion`. So slider movements persist to NVS but have zero runtime effect.

### Changes

| Where | What |
|---|---|
| `dd_ble_cfg.h` (already declares) | `uint32_t dd_ble_cfg_config_version(void)` — keep, it's exactly what we need. |
| `app_controller.cpp::t_cfg_fn` | At the top of every tick, read `dd_ble_cfg_config_version()`; if it differs from `s_last_applied_version`, call `apply_config()` and store. |
| `app_controller.cpp` (new fn) | `static void apply_config(const dd_ble_cfg_t *c)` — translates wire fields into `motion_config_t`, `srv_input` thresholds, `srv_fusion` β; calls each `*_init`. |
| `app_controller_start` | After `dd_ble_cfg_init(nullptr)` returns, call `apply_config(&snapshot)` so the boot config (defaults or NVS-loaded) takes effect before any task runs. |

### `apply_config` (sketch)

```c
static void apply_config(const dd_ble_cfg_t *c) {
    motion_config_t mc = {
        .deadzone_rad = (float)c->deadzone_mrad / 1000.0f,
        .gain_low     = (float)c->sens_x_milli * 0.6f,   /* sens=1.0 → gain_low=600 */
        .gain_exp     = 1.2f,                            /* stays compile-time */
        .velocity_cap = 127.0f,
        .gain_y_scale = (float)c->sens_y_milli / (float)c->sens_x_milli,
    };
    srv_motion_init(&mc);
    srv_input_set_thresholds(c->touch_threshold);
    srv_input_set_debounce_ms(c->debounce_ms);
    srv_fusion_init((float)c->madgwick_beta_milli / 1000.0f);
}
```

Notes:
- `gain_y_scale` is derived from the X/Y sensitivity ratio so the existing `feat/web-app` field stays meaningful.
- `gain_exp` and `velocity_cap` stay compile-time — there's no UI for them in the mock and exposing them is busywork.

### New helpers needed (small additions)

- `srv_input_set_thresholds(const uint16_t thresh[4])` — applies new per-pad thresholds without resetting FSM state.
- `srv_input_set_debounce_ms(uint16_t ms)` — same idea for debounce.
- (already exist) `srv_motion_init`, `srv_fusion_init` — re-entrant per their existing contracts.

---

## 11.2 — Schema v2

Bump `dd_ble_cfg_t` wire format from 10 → **28 bytes**. One step, no fallback.

### Packed wire layout

```
offset  size  field                  units / notes
0       u8    version              = 2
1       u8    flags                bit0 = dirty (unsaved); other bits reserved
2       u16   sens_x_milli         X sensitivity ×1000   (default 1000)
4       u16   sens_y_milli         Y sensitivity ×1000   (default 1000)
6       u16   deadzone_mrad        per-frame angular deadzone, milli-rad (default 4)
8       u16   madgwick_beta_milli  Madgwick β ×1000      (default 50)
10      u16   debounce_ms          input debounce, ms    (default 30)
12      u16   touch_threshold[0]   THUMB  raw-count threshold (default 600)
14      u16   touch_threshold[1]   INDEX  (default 600)
16      u16   touch_threshold[2]   MIDDLE (default 600)
18      u16   touch_threshold[3]   RING   (default 600)
20      u8    click_action[0]      THUMB  → ag_click_action_t (default 0 NONE)
21      u8    click_action[1]      INDEX  (default 1 LEFT)
22      u8    click_action[2]      MIDDLE (default 2 RIGHT)
23      u8    click_action[3]      RING   (default 4 SCROLL_UP — preserves the
                                          existing ring-finger scroll behaviour)
24      u8    modifier_pad         pad index that acts as "shift" (0..3),
                                   or 0xFF to disable (default 0xFF)
25      u8    click_action_alt[0]  THUMB  action while modifier_pad held
26      u8    click_action_alt[1]  INDEX  ditto
27      u8    click_action_alt[2]  MIDDLE ditto
                                   (no alt for the modifier itself)
```

Total: 28 bytes; one ATT MTU-64 packet easily.

### `ag_click_action_t` enum (in `dd_ble_cfg.h`)

```c
typedef enum : uint8_t {
    AG_CLICK_NONE        = 0,
    AG_CLICK_LEFT        = 1,
    AG_CLICK_RIGHT       = 2,
    AG_CLICK_MIDDLE      = 3,
    AG_CLICK_SCROLL_UP   = 4,    /* PRESS = one wheel notch up   */
    AG_CLICK_SCROLL_DOWN = 5,    /* PRESS = one wheel notch down */
    AG_CLICK_CLUTCH      = 6,    /* hold-to-freeze cursor (srv_motion clutch) */
    AG_CLICK_SCROLL_MODE = 7,    /* hold to reroute glove tilt → mouse wheel */
} ag_click_action_t;
```

### Defaults

Picked to preserve current behaviour so the first NVS-empty boot feels identical to `feat/web-app` today:

```
sens_x_milli         = 1000
sens_y_milli         = 1000
deadzone_mrad        = 4         (matches current 0.004f)
madgwick_beta_milli  = 50        (matches current srv_fusion_init(0.05f))
debounce_ms          = 30
touch_threshold[]    = {600, 600, 600, 600}
click_action[]       = {NONE, LEFT, RIGHT, SCROLL_MODE}
                       /* THUMB=NONE, INDEX=LEFT, MIDDLE=RIGHT, RING=SCROLL_MODE
                          — preserves today's hold-ring-to-scroll gesture. */
modifier_pad         = 0xFF
click_action_alt[]   = {NONE, NONE, NONE}
```

### Mapping semantics (kept minimal)

- Each pad fires its `click_action[i]` on PRESS, releases on RELEASE.
- If `modifier_pad != 0xFF` and that pad is currently held, the **other** pads fire `click_action_alt[]` instead of `click_action[]`.
- The modifier pad itself does nothing while held (its `click_action` is suppressed).
- `AG_CLICK_CLUTCH` is special on solo only: PRESS → `srv_motion_set_clutch(true)`, RELEASE → `srv_motion_set_clutch(false)`. Not allowed in `click_action_alt`.
- `AG_CLICK_SCROLL_MODE` is a *hold* gesture: PRESS sets `g_scroll_mode = true`, RELEASE clears it. `t_motion_fn` then reroutes `dy` into the wheel field (existing implementation preserved). This is the data-driven replacement for the hard-coded ring-finger scroll.
- That's it. No multi-pad chords beyond the single modifier. No tap-vs-hold timing.

Example user setup:
- Default: index = left, middle = right, ring = scroll up, thumb = nothing.
- Holding thumb (modifier): index = scroll down, middle = middle-click, ring = (still scrolls up, or remap via alt).

### Where it lives in firmware

The new fields slot into `dd_ble_cfg_t` in `dd_ble_cfg.h`. `dd_ble_cfg.cpp` widens `encode_config` / `decode_config` to the 28-byte layout, bumps `kConfigSize` and the version byte. NVS key stays `"cfg"`; existing v1 blobs will fail the version check and be replaced by defaults on first boot after the upgrade — the no-fallback policy in action.

---

## 11.3 — Click-mapping wiring

The existing PRESS→button logic lives in `t_app_fn` (currently a hard-coded table plus the `g_scroll_mode` atomic). Replace with a small dispatch.

### Resolver

```c
/* in tasks.cpp, file-local */
static uint8_t s_pads_held = 0;   /* bit i = pad i currently pressed (post-debounce) */

static uint8_t resolve_action(uint8_t pad, const dd_ble_cfg_t *c) {
    if (c->modifier_pad <= 3 && c->modifier_pad != pad
        && (s_pads_held & (1u << c->modifier_pad))) {
        /* Alt table is indexed by pad with the modifier slot skipped:
         *   alt[0] = lowest-index non-modifier pad, etc. */
        uint8_t idx = pad < c->modifier_pad ? pad : pad - 1;
        return c->click_action_alt[idx];
    }
    return c->click_action[pad];
}
```

Then in the PRESS/RELEASE handler:

```c
case INPUT_EVT_PRESS:
    s_pads_held |= (1u << ev.pad);
    if (ev.pad == c.modifier_pad) break;     /* modifier emits nothing on press */
    apply_action(resolve_action(ev.pad, &c), /*pressed=*/true);
    break;
case INPUT_EVT_RELEASE:
    s_pads_held &= ~(1u << ev.pad);
    if (ev.pad == c.modifier_pad) break;
    apply_action(resolve_action(ev.pad, &c), /*pressed=*/false);
    break;
```

`apply_action(action, pressed)` translates one enum value into the right operation on `g_current_buttons` / wheel-accumulator / `srv_motion_set_clutch`. Scroll up/down on press are one-shot wheel deltas (re-uses the existing sub-notch accumulator from commit `cdab0fc`).

`c` is a stack snapshot taken at the top of the tick from a thread-safe getter (`dd_ble_cfg_get_config`).

### Retiring `g_scroll_mode`

The hard-coded ring-finger scroll (`g_scroll_mode` atomic) becomes redundant once `click_action[RING] = SCROLL_UP` and the modifier-pad variant covers SCROLL_DOWN. Delete the atomic and its writer to avoid two competing scroll paths.

---

## 11.4 — Web app changes

The `companion/` app already has the connect / tune / calibrate / about screens. Extensions:

| Screen | Add |
|---|---|
| Tune | Slider for `madgwick_beta_milli` (range 0–300, displayed ÷1000), slider for `debounce_ms` (5–200), four per-pad `touch_threshold` sliders with the live touch bar showing the red marker at the current threshold, four `click_action` selectors (segmented control with the 7 enum values as icons), a "Modifier finger" dropdown (None / Thumb / Index / Middle / Ring) that reveals three `click_action_alt` selectors when set. |
| Connect | Show `version` from the config blob; if it isn't 2, render "Firmware too old — please update" and refuse to load the rest of the UI. (No-fallback policy made user-visible.) |
| Codec (`companion/src/ble/codec.ts`) | Widen `encodeConfig` / `decodeConfig` to the new 28-byte layout; bump `CONFIG_VERSION = 2`. Update the round-trip test fixture. |
| Mock client (`companion/src/ble/mockClient.ts`) | Mirror the schema so the dev UX still demos without hardware. |

No new screens, no new BLE characteristics, no protocol changes beyond the widened blob.

---

## 11.5 — Not in this plan

| Item | Why deferred | Where it goes |
|---|---|---|
| Real IMU bias calibration (dwell + motion guard + bias storage + timestamp) | The fake progress bar already lets the UI demo cleanly; promoting it to real behaviour is its own scope (motion guard tuning, time source, validation). | Plan 12 |
| GATT encryption / passkey | Local LAN threat model is low for now; needs an ADR on pairing mode change. | Plan 13 |
| OTA | Transport choice still unresolved. | Plan 14 |
| Per-axis invert flags | The remap commit `f50cc64` should make these unnecessary; revisit only if HIL bring-up shows sign issues that aren't fixable in `srv_motion`. | Backlog |
| Stored profiles (gaming / pointing) | Web app can hold profiles client-side and write the chosen one. | Out of scope |
| MTU negotiation hardening | 28-byte config and ~24-byte telemetry fit current MTU; revisit only if a host caps lower. | Backlog |
| Schema-info characteristic | Version byte at offset 0 already serves the same purpose. | Dropped |
| Two-blob NVS crash-safety | Single-key write is good enough — if a crash mid-write loses tuning, the cost is "redo the sliders". | Backlog |

---

## 11.6 — Test plan

Native (`env:native`):

- `test_codec_v2_roundtrip` — encode → decode of a populated `dd_ble_cfg_t` returns the same struct.
- `test_codec_rejects_v1_size` — feeding a 10-byte blob to `decode_config` returns false.
- `test_resolve_action_solo` — no modifier held → `resolve_action` returns `click_action[pad]`.
- `test_resolve_action_with_modifier` — modifier held → returns `click_action_alt[shifted_index]`.
- `test_apply_config_propagates` — calling `apply_config` with sens_x=2000 changes the value seen by a subsequent `srv_motion_update`.

On-target (`env:esp32dev`):

- HIL #1: open app, move sens_x slider → cursor speed visibly changes within ~150 ms.
- HIL #2: change `click_action[INDEX]` to RIGHT in the app → tapping index now right-clicks immediately.
- HIL #3: set modifier_pad = THUMB, `click_action_alt[INDEX] = SCROLL_DOWN`. Tap index alone → left-click. Hold thumb + tap index → page scrolls down. Release thumb → next index tap is left-click again.
- HIL #4: set threshold[INDEX] aggressively low → INDEX triggers on a near-miss; raise it → only firm taps trigger. Watch the live touch bar's red marker move with the slider.
- HIL #5: power-cycle after Save → settings persist.
- HIL #6: connect old (v1) companion build to v2 firmware → app shows the "firmware too old" wall (no-fallback policy works as advertised).

---

## 11.7 — Effort estimate

| Section | LOC est. | Days |
|---|---|---|
| 11.1 apply loop + `srv_input` setters | 80 | 0.5 |
| 11.2 schema v2 (firmware codec + NVS bump) | 120 | 0.5 |
| 11.3 click-mapping dispatch + retire `g_scroll_mode` | 120 | 0.5 |
| 11.4 web app (sliders, mapping UI, codec, mock) | 250 | 1 |
| Tests | 200 | 0.5 |
| **Total** | ~770 | ~3 dev-days |

Roughly 1/4 the size of the original Plan 10 draft — because `feat/web-app` already did the heavy plumbing.

---

## 11.8 — Plan 09 follow-up (not part of this plan, but blocks it)

Once `feat/web-app` rebases on `f50cc64` (90° rotation commit), re-run the HIL bring-up checklist §8 with attention to:

1. `srv_motion`'s `kYawToX` sign and `theta_for_y` sign — both were calibrated against the pre-rotation axis frame.
2. The deadzone constant (`0.004f`) — may shift slightly with the new IMU frame.
3. Ring-finger scroll direction — same caveat once `g_scroll_mode` is replaced by `click_action[RING]`.
