#pragma once
/* srv_input — touch-pad debounce and edge-detection service.
 *
 * Consumes one touch_sample_t per call (typically at 100 Hz from the
 * t_touch task) and emits zero-or-more PRESS/RELEASE events per pad into
 * a caller-supplied buffer. A four-state per-pad FSM filters single-tick
 * glitches both on press and on release.
 *
 * Thread-safety: NOT thread-safe. Call exclusively from t_app task.
 * Platform:      Pure C++ (stdlib only). No Arduino or ESP-IDF includes.
 *
 * Note: `touch_sample_t` / `touch_pad_id_t` / `TOUCH_PAD_COUNT` are
 * defined in ag_types.h (not dd_touch.h), so this lib depends only on
 * app_config — the dd_touch driver is not on the include path.
 */

#include "ag_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    INPUT_EVT_NONE    = 0,
    INPUT_EVT_PRESS,
    INPUT_EVT_RELEASE
} input_evt_kind_t;

typedef struct {
    touch_pad_id_t   pad;
    input_evt_kind_t kind;
    uint64_t         t_us;   /* timestamp of the sample that committed the event */
} input_event_t;

/* Initialise or re-initialise the per-pad FSMs and chord book-keeping.
 * `debounce_ms` is rounded up to a whole number of 10 ms sample ticks
 * (the assumed t_touch period) and clamped to >= 1 tick. Returns AG_OK. */
ag_result_t srv_input_init(uint16_t debounce_ms);

/* Consume one touch sample; emit 0..TOUCH_PAD_COUNT events into `out[]`.
 *
 * s        Pointer to a valid touch_sample_t.
 * out      Caller-owned buffer. May be NULL when out_cap == 0.
 * out_cap  Capacity of `out` in events. A capacity < TOUCH_PAD_COUNT
 *          risks silently dropping overflow events on the same tick.
 * out_len  Receives the number of events written on success (always
 *          reset to 0 at the top of the call).
 *
 * Returns AG_OK on success, AG_ERR_ARG on NULL `s` / `out_len`, or on
 * NULL `out` when `out_cap > 0`. Never blocks, never allocates. */
ag_result_t srv_input_process(const touch_sample_t *s,
                              input_event_t *out,
                              size_t out_cap,
                              size_t *out_len);

/* Force every pad FSM back to IDLE and clear chord state. Config
 * (debounce ticks) is preserved. */
void srv_input_reset(void);

#ifdef __cplusplus
}
#endif
