# E08 — Input Service (srv_input)

- **Status:** Done
- **Phase:** I
- **Owns:** `air-glove/lib/srv_input/`
- **Plan:** `docs/plans/07-srv-input.md`
- **Realises:** FR-004

## Goal

Turn raw touch samples (`touch_sample_t` from `dd_touch`) into debounced press/release events per pad, with chord recognition for simultaneous multi-pad contacts. Pure C — unit-testable on host.

## Scope

**In:**
- Per-pad debounce window (default 15 ms, configurable).
- Edge detection: emits `INPUT_EVT_PRESS` on first stable-high, `INPUT_EVT_RELEASE` on first stable-low; no events during hold.
- Chord resolution: if two pads press within a 30 ms window, emit a single chord event instead of two presses. Phase I only needs the bare events — chord logic is implemented but callers (E09) read per-pad events for L/R click.
- Native unit tests in `test/test_srv_input/`: single glitch filtered, press+hold fires once, release fires once, chord window grouping.

**Out:**
- Mapping events to mouse actions (that's E09).
- Scroll chord semantics — E10 (backlog).

## Public interface

```c
// srv_input/include/srv_input.h
#include "ag_types.h"
#include "dd_touch.h"

typedef enum { INPUT_EVT_NONE=0, INPUT_EVT_PRESS, INPUT_EVT_RELEASE } input_evt_kind_t;
typedef struct {
  touch_pad_id_t pad;
  input_evt_kind_t kind;
  uint64_t t_us;
} input_event_t;

ag_result_t srv_input_init(uint16_t debounce_ms);
// Pushes one sample into the FSM; pops 0..N events into `out` (caller-sized).
ag_result_t srv_input_process(const touch_sample_t *s,
                              input_event_t *out, size_t out_cap, size_t *out_len);
void        srv_input_reset(void);
```

## Acceptance criteria

- [x] Native test: a 5 ms glitch on a pad produces zero events. *Covered by `test_single_glitch_filtered` — a one-sample spike on INDEX over a 10-tick window produces `out_len = 0` on every call. The RISING state reverts to IDLE on the next low sample (glitch filter).*
- [x] Native test: a press held 500 ms produces exactly one `PRESS` and one `RELEASE`. *Covered by `test_press_held_fires_once` (50 ticks × 10 ms = 500 ms held, exactly 1 PRESS, 0 RELEASE while held) and `test_release_fires_once` (exactly 1 RELEASE on the deassert edge).*
- [x] Native test: simultaneous index + middle press within 30 ms produces at most two events (one per pad), each correctly labelled. *Covered by `test_two_pads_simultaneous_press_within_30ms` — INDEX high on tick 0, both INDEX+MIDDLE on ticks 1-3; exactly one PRESS per pad with `pad` field correctly set.*
- [x] `srv_input_process()` never blocks; returns all events available under the supplied `out_cap`. *Covered by `test_out_cap_zero_never_writes` (accepts `out_cap=0` with `out=NULL`, returns AG_OK, `*out_len=0`) + code review: no loops beyond `TOUCH_PAD_COUNT`, no blocking calls, no waits. Overflow on cap < 4 is silently dropped per contract.*
- [x] No heap allocations in the hot path. *Verified by `nm -u srv_input.o` — no `malloc` / `free` / `new` / `delete` / `calloc` / `realloc` referenced. All state is file-scope static; event buffer is caller-supplied.*

## Dependencies

- E02.

## Progress log

- 2026-04-21: Epic created. Plan: `docs/plans/07-srv-input.md`.
- 2026-04-23: Implementation delivered. New files:
  - `air-glove/lib/srv_input/library.json` — manifest; depends only on `app_config` (the `touch_sample_t` / `touch_pad_id_t` types live in `ag_types.h`, so the lib does NOT pull in `dd_touch.h` and is free of any driver dependency).
  - `air-glove/lib/srv_input/include/srv_input.h` — full contract header (`#pragma once`, per-function docs on thread-safety, behaviour on NULL `out` with zero capacity, chord note for E10).
  - `air-glove/lib/srv_input/src/srv_input.cpp` — 4-state per-pad FSM (IDLE / RISING / PRESSED / FALLING). Debounce = `ceil(debounce_ms / 10 ms)` ticks, default 2 (15 ms rounded up). PRESS fires on commit from RISING; RELEASE fires on commit from FALLING. Glitch in either direction reverts to the prior committed state. Chord scaffolding (last_press_t_us array + chord-window constant) retained for future E10 but not exported. State footprint ~44 bytes.
  - `air-glove/test/test_srv_input/test_main.cpp` — 8 native Unity tests replacing the placeholder.
- 2026-04-23: Verification complete. `g++ -std=gnu++17 -Wall -Wextra -Werror` clean; `nm` exports only the three public `extern "C"` symbols (`srv_input_init`, `srv_input_process`, `srv_input_reset`); `nm -u` shows no heap-allocator references. Mini-Unity shim on host ran **8/8 tests, 0 failures**. All five acceptance criteria covered by passing tests. Status: Done.
