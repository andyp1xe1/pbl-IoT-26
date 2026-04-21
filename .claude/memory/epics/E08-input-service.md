# E08 — Input Service (srv_input)

- **Status:** Not Started
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

- [ ] Native test: a 5 ms glitch on a pad produces zero events.
- [ ] Native test: a press held 500 ms produces exactly one `PRESS` and one `RELEASE`.
- [ ] Native test: simultaneous index + middle press within 30 ms produces at most two events (one per pad), each correctly labelled.
- [ ] `srv_input_process()` never blocks; returns all events available under the supplied `out_cap`.
- [ ] No heap allocations in the hot path.

## Dependencies

- E02.

## Progress log

- 2026-04-21: Epic created. Plan: `docs/plans/07-srv-input.md`.
