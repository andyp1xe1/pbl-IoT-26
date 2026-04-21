# Plan 07 — srv_input (touch debounce + edge events)

- **Epic:** [E08](../../.claude/memory/epics/E08-input-service.md)
- **Goal:** Translate raw `touch_sample_t` into debounced per-pad `INPUT_EVT_PRESS` / `INPUT_EVT_RELEASE` events using a per-pad finite-state machine. Pure C; builds and tests under `env:native`. Chord code paths exist but callers read per-pad events in Phase I.
- **Preconditions:** E02 types (`ag_result_t`, `AG_OK`, `AG_ERR_ARG`). E04 `touch_sample_t`/`touch_pad_id_t` / `TOUCH_PAD_COUNT = 4`. Plan 01 scaffolded `air-glove/lib/srv_input/`.

## Files to create

| Path (under `air-glove/lib/srv_input/`) | Role |
|---|---|
| `include/srv_input.h` | Public header (event enum, event struct, init/process/reset). |
| `src/srv_input.cpp` | Per-pad FSM + chord book-keeping. |
| `library.json` | Lib manifest, `native` + `esp32dev`. |
| `test/test_srv_input/test_main.cpp` | Native Unity tests. |

## Step-by-step

1. In `srv_input.h`, declare the event enum, `input_event_t`, and the three functions verbatim from the epic. Include `"dd_touch.h"` for `touch_sample_t` / `touch_pad_id_t`.
2. In `srv_input.cpp`, define per-pad FSM:
   - `typedef enum { PAD_IDLE, PAD_RISING, PAD_PRESSED, PAD_FALLING } pad_state_t;`
   - File-scope state arrays (one entry per pad): `static pad_state_t s_state[TOUCH_PAD_COUNT];`, `static uint16_t s_counter[TOUCH_PAD_COUNT];` (ticks accumulated in current transition), `static uint64_t s_last_press_t_us[TOUCH_PAD_COUNT];` (for chord window).
   - Config: `static uint16_t s_debounce_ticks = 2;` (default 15 ms / 10 ms sample ≈ 2 ticks, rounded up; init function recomputes from ms).
   - Constant: `static const uint16_t kChordWindowTicks = 3;` (30 ms).
3. Implement `srv_input_init(uint16_t debounce_ms)`:
   - Assume sample period = 10 ms (driven by `t_touch` in `app_controller`). Store `s_debounce_ticks = (debounce_ms + 9) / 10;` and clamp to `>= 1`.
   - Clear `s_state[i] = PAD_IDLE; s_counter[i] = 0; s_last_press_t_us[i] = 0;` for each pad.
   - Return `AG_OK`.
4. Implement `srv_input_reset(void)` — identical to the state-clearing block.
5. Implement `srv_input_process(const touch_sample_t *s, input_event_t *out, size_t out_cap, size_t *out_len)`:
   1. Null-check args → `AG_ERR_ARG`.
   2. `*out_len = 0;`
   3. For each pad `i` in 0..3:
      1. `bool raw_touched = (s->touched_mask & (1u << i)) != 0;`
      2. FSM step:
         - `PAD_IDLE` + raw high → transition to `PAD_RISING`, `counter = 1`.
         - `PAD_RISING` + raw high → `counter++;` if `counter >= s_debounce_ticks` → commit: `s_state[i] = PAD_PRESSED; s_last_press_t_us[i] = s->t_us;` and emit `INPUT_EVT_PRESS`.
         - `PAD_RISING` + raw low → back to `PAD_IDLE; counter = 0;` (glitch filtered).
         - `PAD_PRESSED` + raw low → `PAD_FALLING; counter = 1;`.
         - `PAD_PRESSED` + raw high → no-op (holding).
         - `PAD_FALLING` + raw low → `counter++;` if `counter >= s_debounce_ticks` → commit: `s_state[i] = PAD_IDLE;` emit `INPUT_EVT_RELEASE`.
         - `PAD_FALLING` + raw high → back to `PAD_PRESSED; counter = 0;` (release glitch).
      3. Emit helper: if event to emit and `*out_len < out_cap`, write `out[*out_len] = { .pad = (touch_pad_id_t)i, .kind = kind, .t_us = s->t_us };` and `++*out_len;`. If `*out_len >= out_cap`, silently drop overflow (caller is expected to size `out_cap` ≥ 4 for Phase I since the bus produces at most 4 events per tick).
   4. Chord book-keeping (kept for future E10 use): after the per-pad loop, compare `s_last_press_t_us[INDEX]` and `s_last_press_t_us[MIDDLE]`. If both were set within the current tick AND `|Δt| <= kChordWindowTicks * 10000` µs, set a chord flag (static bool) to be consumed by a later `srv_input_get_chord()` — NOT implemented in Phase I per epic scope. Leave a commented stub.
   5. Return `AG_OK`.
6. Heap-free: every buffer is caller-supplied or fixed-size static. No `new`/`malloc`.

## Public header (target signature)

```c
// lib/srv_input/include/srv_input.h
#pragma once
#include "ag_types.h"
#include "dd_touch.h"

typedef enum {
  INPUT_EVT_NONE = 0,
  INPUT_EVT_PRESS,
  INPUT_EVT_RELEASE
} input_evt_kind_t;

typedef struct {
  touch_pad_id_t   pad;
  input_evt_kind_t kind;
  uint64_t         t_us;
} input_event_t;

// Initialise FSM. Sample period is assumed 10 ms; debounce_ms is rounded up.
ag_result_t srv_input_init(uint16_t debounce_ms);

// Consume one touch sample. Emits 0..N press/release events into out[]
// (N <= out_cap). Sets *out_len to the number written. Does not block.
// Thread-safety: single-caller (t_app).
ag_result_t srv_input_process(const touch_sample_t *s,
                              input_event_t *out,
                              size_t out_cap,
                              size_t *out_len);

// Reset all pads to IDLE, clear chord history.
void        srv_input_reset(void);
```

## Internal design notes

- FSM chosen over a simple "N consecutive samples agree" counter because it naturally generates events only on commit, handles glitch release separately from glitch press, and matches the four-state transition diagram in the Layer 1 testing strategy §3.
- `s_debounce_ticks = ceil(debounce_ms / 10)` — conservative rounding means the user's configured window is the minimum, not the average.
- Chord detection scaffolding is kept because E10 (scroll, clutch) will activate it without reworking the FSM; the cost in Phase I is one extra u64 array.
- No floats, no division in the hot path.
- State footprint: 4·(1 + 2 + 8) = 44 bytes plus config. Well under the 2 KB `t_app` stack budget.

## Verification

Native Unity tests (`test/test_srv_input/test_main.cpp`), runs under `env:native`:

- `test_init_returns_ok` — `srv_input_init(15)` returns `AG_OK`.
- `test_process_null_args_rejected` — null `s` or `out` or `out_len` → `AG_ERR_ARG`.
- `test_single_glitch_filtered` — feed a single-tick touch on INDEX (one sample with bit set, surrounded by bits clear); over 10 ticks assert `*out_len == 0` across all calls.
- `test_press_held_fires_once` — feed 50 ticks with INDEX set (500 ms); assert exactly one `INPUT_EVT_PRESS` emitted on tick 2 (debounce = 15 ms rounded to 2 ticks), no further events while held, and one `INPUT_EVT_RELEASE` on tick 2 after the input goes low.
- `test_release_fires_once` — direct assertion extracted from the above.
- `test_two_pads_simultaneous_press_within_30ms` — feed INDEX set on tick 0 and MIDDLE set on tick 1; after debounce, assert both emit exactly one `PRESS`, in pad-order, with correct `pad` field.
- `test_out_cap_zero_never_writes` — `out_cap = 0` → function returns `AG_OK` with `*out_len == 0`, no OOB write (run under address-sanitizer via `build_flags = -fsanitize=address`).
- `test_reset_returns_to_idle` — after a pending press is in progress (one tick of RISING), `srv_input_reset()` discards the in-flight transition; next call with raw low yields no event.
- `test_no_heap_allocations` — not directly testable in Unity; assert via a linker check that the lib pulls in zero malloc symbols (document in test notes rather than code-asserted).

PlatformIO command:

```
pio test -e native -f test_srv_input
```

## Rollback / risk

- Smallest revert: stub `srv_input_process()` to `*out_len = 0; return AG_OK;`. Firmware runs; clicks never fire. (Motion still works.)
- Known risks:
  - Debounce rounding cuts a 10 ms setting to 10 ms (1 tick) — that is insufficient to reject ISR-induced glitches on long wires. Default of 15 ms is the minimum recommended; do not go lower in `app_config`.
  - Chord stub is dead code in Phase I — must not leak events. Wrapped behind a static flag that is never read yet; remove in code review if any reviewer flags it.

## References

- architecture.md §4.1 (task `t_app` consuming `q_buttons`), §6.2 (touch path)
- decisions.md ADR-003 (native touch sourcing), ADR-007 (Phase I scope — chord deferred)
- epic E08 (public interface, acceptance criteria)
- testing-strategy.md §3 (`srv_input` test enumeration)
