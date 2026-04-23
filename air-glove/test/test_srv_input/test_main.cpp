/* test_srv_input — native Unity tests for the debounce + edge service.
 *
 * Run with:  pio test -e native -f test_srv_input
 *
 * Sample period is assumed to be 10 ms (the t_touch cadence). All tests
 * construct `touch_sample_t` values by hand — no hardware required.
 */

#include <unity.h>
#include <stddef.h>
#include <stdint.h>

#include "srv_input.h"

/* ── Helpers ──────────────────────────────────────────────────────────── */

static touch_sample_t make_touch(uint8_t mask, uint64_t t_us)
{
    touch_sample_t s;
    for (int i = 0; i < TOUCH_PAD_COUNT; ++i) s.raw[i] = 50;  /* arbitrary */
    s.touched_mask = mask;
    s.t_us         = t_us;
    return s;
}

static uint64_t tick_us(int tick /* 0-based */)
{
    /* Give tick 0 a non-zero timestamp so tests can distinguish "never set". */
    return (uint64_t)(tick + 1) * 10000u;
}

static int count_events(const input_event_t *ev, size_t n, input_evt_kind_t kind)
{
    int c = 0;
    for (size_t i = 0; i < n; ++i) if (ev[i].kind == kind) c++;
    return c;
}

/* ── Fixture ──────────────────────────────────────────────────────────── */

void setUp(void)    { srv_input_init(15); }
void tearDown(void) {}

/* ── Tests ────────────────────────────────────────────────────────────── */

/* 1. Basic init contract. */
static void test_init_returns_ok(void)
{
    TEST_ASSERT_EQUAL_INT(AG_OK, srv_input_init(15));
    TEST_ASSERT_EQUAL_INT(AG_OK, srv_input_init(0));     /* clamped to 1 tick */
    TEST_ASSERT_EQUAL_INT(AG_OK, srv_input_init(10000)); /* any value accepted */
}

/* 2. NULL arguments rejected. */
static void test_process_null_args_rejected(void)
{
    input_event_t  out[4];
    size_t         out_len = 0;
    touch_sample_t s       = make_touch(0, tick_us(0));

    TEST_ASSERT_EQUAL_INT(AG_ERR_ARG, srv_input_process(nullptr, out, 4, &out_len));
    TEST_ASSERT_EQUAL_INT(AG_ERR_ARG, srv_input_process(&s, nullptr, 4, &out_len));
    TEST_ASSERT_EQUAL_INT(AG_ERR_ARG, srv_input_process(&s, out, 4, nullptr));
}

/* 3. A single-tick glitch on INDEX must NOT emit any event. */
static void test_single_glitch_filtered(void)
{
    input_event_t out[8];
    size_t        out_len = 0;
    int           total   = 0;

    const uint8_t seq[10] = {
        0, 0,
        (uint8_t)(1u << TOUCH_PAD_INDEX),   /* glitch */
        0, 0, 0, 0, 0, 0, 0
    };
    for (int t = 0; t < 10; ++t) {
        touch_sample_t s = make_touch(seq[t], tick_us(t));
        out_len = 0;
        TEST_ASSERT_EQUAL_INT(AG_OK, srv_input_process(&s, out, 8, &out_len));
        total += (int)out_len;
    }
    TEST_ASSERT_EQUAL_INT(0, total);
}

/* 4. Press held for 50 ticks → exactly one PRESS; no RELEASE while held. */
static void test_press_held_fires_once(void)
{
    input_event_t out[8];
    size_t        out_len = 0;
    int           presses   = 0;
    int           releases  = 0;

    for (int t = 0; t < 50; ++t) {
        touch_sample_t s = make_touch((uint8_t)(1u << TOUCH_PAD_INDEX), tick_us(t));
        out_len = 0;
        srv_input_process(&s, out, 8, &out_len);
        presses  += count_events(out, out_len, INPUT_EVT_PRESS);
        releases += count_events(out, out_len, INPUT_EVT_RELEASE);
    }
    TEST_ASSERT_EQUAL_INT(1, presses);
    TEST_ASSERT_EQUAL_INT(0, releases);
}

/* 5. After a held press, releasing yields exactly one RELEASE. */
static void test_release_fires_once(void)
{
    input_event_t out[8];
    size_t        out_len = 0;

    /* Hold INDEX for 4 ticks — PRESS commits on tick 1 (2nd high tick). */
    for (int t = 0; t < 4; ++t) {
        touch_sample_t s = make_touch((uint8_t)(1u << TOUCH_PAD_INDEX), tick_us(t));
        out_len = 0;
        srv_input_process(&s, out, 8, &out_len);
    }

    int releases = 0;
    for (int t = 4; t < 10; ++t) {
        touch_sample_t s = make_touch(0, tick_us(t));
        out_len = 0;
        srv_input_process(&s, out, 8, &out_len);
        releases += count_events(out, out_len, INPUT_EVT_RELEASE);
    }
    TEST_ASSERT_EQUAL_INT(1, releases);
}

/* 6. Simultaneous press on INDEX (tick 0) and MIDDLE (tick 1) within the
 *    30 ms chord window must yield exactly one PRESS per pad with the
 *    correct `pad` field. */
static void test_two_pads_simultaneous_press_within_30ms(void)
{
    input_event_t out[8];
    size_t        out_len = 0;

    const uint8_t seq[4] = {
        (uint8_t) (1u << TOUCH_PAD_INDEX),
        (uint8_t)((1u << TOUCH_PAD_INDEX) | (1u << TOUCH_PAD_MIDDLE)),
        (uint8_t)((1u << TOUCH_PAD_INDEX) | (1u << TOUCH_PAD_MIDDLE)),
        (uint8_t)((1u << TOUCH_PAD_INDEX) | (1u << TOUCH_PAD_MIDDLE)),
    };

    int index_presses  = 0;
    int middle_presses = 0;
    for (int t = 0; t < 4; ++t) {
        touch_sample_t s = make_touch(seq[t], tick_us(t));
        out_len = 0;
        srv_input_process(&s, out, 8, &out_len);
        for (size_t j = 0; j < out_len; ++j) {
            if (out[j].kind == INPUT_EVT_PRESS) {
                if (out[j].pad == TOUCH_PAD_INDEX)  index_presses++;
                if (out[j].pad == TOUCH_PAD_MIDDLE) middle_presses++;
            }
        }
    }
    TEST_ASSERT_EQUAL_INT(1, index_presses);
    TEST_ASSERT_EQUAL_INT(1, middle_presses);
}

/* 7. `out_cap == 0` must never write through `out`. We pass NULL out on
 *    purpose — the contract allows it for zero-capacity polling. */
static void test_out_cap_zero_never_writes(void)
{
    size_t out_len = 99;
    for (int t = 0; t < 10; ++t) {
        touch_sample_t s = make_touch((uint8_t)(1u << TOUCH_PAD_INDEX), tick_us(t));
        out_len = 99;
        ag_result_t r = srv_input_process(&s, nullptr, 0, &out_len);
        TEST_ASSERT_EQUAL_INT(AG_OK, r);
        TEST_ASSERT_EQUAL_INT(0, (int)out_len);
    }
}

/* 8. Reset during a pending RISING discards the transition — no PRESS fires
 *    from the pre-reset high sample. */
static void test_reset_returns_to_idle(void)
{
    input_event_t out[8];
    size_t        out_len = 0;

    /* One tick of INDEX high: enters RISING, no event yet. */
    touch_sample_t s1 = make_touch((uint8_t)(1u << TOUCH_PAD_INDEX), tick_us(0));
    srv_input_process(&s1, out, 8, &out_len);
    TEST_ASSERT_EQUAL_INT(0, (int)out_len);

    srv_input_reset();

    /* Low sample after reset: IDLE + low → IDLE, no event. */
    touch_sample_t s2 = make_touch(0, tick_us(1));
    out_len = 0;
    srv_input_process(&s2, out, 8, &out_len);
    TEST_ASSERT_EQUAL_INT(0, (int)out_len);

    /* Feed two high ticks — should still debounce normally after reset. */
    touch_sample_t s3 = make_touch((uint8_t)(1u << TOUCH_PAD_INDEX), tick_us(2));
    out_len = 0;
    srv_input_process(&s3, out, 8, &out_len);
    TEST_ASSERT_EQUAL_INT(0, (int)out_len);           /* 1st high: RISING */

    touch_sample_t s4 = make_touch((uint8_t)(1u << TOUCH_PAD_INDEX), tick_us(3));
    out_len = 0;
    srv_input_process(&s4, out, 8, &out_len);
    TEST_ASSERT_EQUAL_INT(1, (int)out_len);           /* 2nd high: PRESS */
    TEST_ASSERT_EQUAL_INT(INPUT_EVT_PRESS, (int)out[0].kind);
    TEST_ASSERT_EQUAL_INT(TOUCH_PAD_INDEX, (int)out[0].pad);
}

/* ── Entry point ──────────────────────────────────────────────────────── */

int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_init_returns_ok);
    RUN_TEST(test_process_null_args_rejected);
    RUN_TEST(test_single_glitch_filtered);
    RUN_TEST(test_press_held_fires_once);
    RUN_TEST(test_release_fires_once);
    RUN_TEST(test_two_pads_simultaneous_press_within_30ms);
    RUN_TEST(test_out_cap_zero_never_writes);
    RUN_TEST(test_reset_returns_to_idle);
    return UNITY_END();
}
