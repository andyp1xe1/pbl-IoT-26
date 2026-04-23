/* srv_input — per-pad debounce FSM. Emits PRESS on stable rise, RELEASE
 * on stable fall, filters single-tick glitches in either direction.
 *
 * States per pad:
 *     IDLE  ──high──▶ RISING   ──high(N ticks)──▶ PRESSED  (emit PRESS)
 *            ◀─low──          ◀─low (glitch)─
 *
 *     PRESSED ──low──▶ FALLING ──low (N ticks)──▶ IDLE     (emit RELEASE)
 *               ◀─high──      ◀─high(glitch)─
 *
 * Where N = s_debounce_ticks = ceil(debounce_ms / 10 ms), clamped >= 1.
 *
 * Pure C++ (stdlib only). NO Arduino / ESP-IDF includes.
 */

#include "srv_input.h"

/* ── FSM per-pad state ────────────────────────────────────────────────── */

typedef enum {
    PAD_IDLE = 0,
    PAD_RISING,
    PAD_PRESSED,
    PAD_FALLING
} pad_state_t;

static pad_state_t s_state  [TOUCH_PAD_COUNT];
static uint16_t    s_counter[TOUCH_PAD_COUNT];
static uint64_t    s_last_press_t_us[TOUCH_PAD_COUNT];

/* Default debounce = 15 ms → 2 ticks at the assumed 10 ms sample period. */
static uint16_t    s_debounce_ticks = 2;

/* Chord window (future E10). Kept as static data so callers do not pay
 * for it until a chord API is introduced in Phase II. */
static const uint16_t kChordWindowTicks __attribute__((unused)) = 3; /* 30 ms */
static bool           s_chord_flag      __attribute__((unused)) = false;

/* ── Helpers ──────────────────────────────────────────────────────────── */

static void clear_state_all(void)
{
    for (size_t i = 0; i < TOUCH_PAD_COUNT; ++i) {
        s_state  [i] = PAD_IDLE;
        s_counter[i] = 0;
        s_last_press_t_us[i] = 0;
    }
    s_chord_flag = false;
}

static inline void emit(input_event_t *out, size_t out_cap, size_t *out_len,
                        touch_pad_id_t pad, input_evt_kind_t kind, uint64_t t_us)
{
    if (*out_len < out_cap) {
        out[*out_len].pad  = pad;
        out[*out_len].kind = kind;
        out[*out_len].t_us = t_us;
        (*out_len)++;
    }
    /* Overflow is silently dropped: caller is expected to size out_cap
     * >= TOUCH_PAD_COUNT so all pad events on a single tick fit. */
}

/* ── Public API ───────────────────────────────────────────────────────── */

extern "C" ag_result_t srv_input_init(uint16_t debounce_ms)
{
    uint32_t ticks = ((uint32_t)debounce_ms + 9u) / 10u;  /* round up */
    if (ticks < 1u) ticks = 1u;
    s_debounce_ticks = (uint16_t)ticks;
    clear_state_all();
    return AG_OK;
}

extern "C" void srv_input_reset(void)
{
    clear_state_all();
}

extern "C" ag_result_t srv_input_process(const touch_sample_t *s,
                                         input_event_t *out,
                                         size_t out_cap,
                                         size_t *out_len)
{
    if (!s || !out_len)              return AG_ERR_ARG;
    if (out_cap > 0 && out == NULL)  return AG_ERR_ARG;
    *out_len = 0;

    for (uint8_t i = 0; i < TOUCH_PAD_COUNT; ++i) {
        const bool raw_high = (s->touched_mask & (uint8_t)(1u << i)) != 0;

        switch (s_state[i]) {
        case PAD_IDLE:
            if (raw_high) {
                s_state  [i] = PAD_RISING;
                s_counter[i] = 1;
            }
            break;

        case PAD_RISING:
            if (raw_high) {
                s_counter[i]++;
                if (s_counter[i] >= s_debounce_ticks) {
                    s_state  [i] = PAD_PRESSED;
                    s_counter[i] = 0;
                    s_last_press_t_us[i] = s->t_us;
                    emit(out, out_cap, out_len,
                         (touch_pad_id_t)i, INPUT_EVT_PRESS, s->t_us);
                }
            } else {
                /* Glitch filtered — one-sample blip, back to idle. */
                s_state  [i] = PAD_IDLE;
                s_counter[i] = 0;
            }
            break;

        case PAD_PRESSED:
            if (!raw_high) {
                s_state  [i] = PAD_FALLING;
                s_counter[i] = 1;
            }
            /* else: holding — no event emitted. */
            break;

        case PAD_FALLING:
            if (!raw_high) {
                s_counter[i]++;
                if (s_counter[i] >= s_debounce_ticks) {
                    s_state  [i] = PAD_IDLE;
                    s_counter[i] = 0;
                    emit(out, out_cap, out_len,
                         (touch_pad_id_t)i, INPUT_EVT_RELEASE, s->t_us);
                }
            } else {
                /* Release glitch — stay pressed. */
                s_state  [i] = PAD_PRESSED;
                s_counter[i] = 0;
            }
            break;

        default:
            /* Unreachable. Defensive recovery: snap to idle. */
            s_state  [i] = PAD_IDLE;
            s_counter[i] = 0;
            break;
        }
    }

    /* Chord book-keeping (future E10 hook).
     * TODO(E10): compare last_press_t_us[INDEX] and last_press_t_us[MIDDLE];
     * if both fall within kChordWindowTicks * 10 000 µs of s->t_us, set
     * s_chord_flag true for a later srv_input_get_chord() API.
     * Phase I intentionally emits no chord event. */
    (void)kChordWindowTicks;
    (void)s_chord_flag;

    return AG_OK;
}
