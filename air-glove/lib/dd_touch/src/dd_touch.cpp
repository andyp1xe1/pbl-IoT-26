/* dd_touch — hybrid input driver.
 *
 * THUMB (GPIO4)  : ESP32 native capacitive-touch (`touchRead()`), used as a
 *                  gesture reference pad on the glove palm/thumb.
 * INDEX (GPIO14) : tactile push-button wired between GPIO and GND, read via
 * MIDDLE (GPIO15)  `digitalRead()` with INPUT_PULLUP.  Button pressed = LOW.
 * RING  (GPIO13)
 *
 * The button pads bypass the capacitive peripheral entirely — no calibration,
 * no EMA baseline, no threshold arithmetic.  srv_input debounces all pads
 * identically (2-tick FSM), so no changes are needed above this layer.
 *
 * ADR-005 permits the Arduino include below because this is a dd_* lib.
 *
 * All state is file-scope `static`; only the two public entry points
 * are exported.
 */

#include <Arduino.h>
#include <esp_timer.h>
#include <stdio.h>

#include "dd_touch.h"
#include "ag_pins.h"

namespace {

/* GPIO per `touch_pad_id_t` — indices MUST align with the enum. */
static const uint8_t kGpio[TOUCH_PAD_COUNT] = { 4, 14, 15, 13 };

/* Pads backed by physical buttons (INPUT_PULLUP, active-LOW).
 * Bit N = pad N uses digitalRead instead of touchRead. */
static constexpr uint8_t kButtonMask =
    (1u << TOUCH_PAD_INDEX) | (1u << TOUCH_PAD_MIDDLE) | (1u << TOUCH_PAD_RING);

/* ── Capacitive-touch settings (THUMB only) ───────────────────────────── */

/* Touch fires when raw < baseline * kThreshRatio.
 * 0.85 = 15% capacitance drop. */
static constexpr float   kThreshRatio  = 0.85f;
static constexpr float   kEmaAlpha     = 0.05f;   /* ~0.2 s drift adaptation */
static constexpr uint8_t kCalibSamples = 50;

static uint16_t s_baseline [TOUCH_PAD_COUNT];
static uint16_t s_threshold[TOUCH_PAD_COUNT];
static bool     s_initialized = false;

static inline uint16_t apply_ratio(uint16_t b) {
    return (uint16_t)((float)b * kThreshRatio);
}

static inline bool is_button(uint8_t i) {
    return (kButtonMask & (uint8_t)(1u << i)) != 0;
}

} /* namespace */

extern "C" ag_result_t dd_touch_init(void) {
    /* Configure button GPIOs as digital inputs with internal pull-up.
     * The button connects GPIO to GND; released = HIGH, pressed = LOW. */
    for (uint8_t i = 0; i < TOUCH_PAD_COUNT; ++i) {
        if (is_button(i)) {
            pinMode(kGpio[i], INPUT_PULLUP);
            s_baseline [i] = 0;
            s_threshold[i] = 0;
        }
    }

    /* Capacitive calibration for non-button pads (THUMB only).
     * Prime the peripheral first — first reading after boot is often 0. */
    for (uint8_t i = 0; i < TOUCH_PAD_COUNT; ++i) {
        if (!is_button(i)) {
            (void)touchRead(kGpio[i]);
        }
    }
    delay(10);

    bool cap_wiring_ok = true;
    for (uint8_t i = 0; i < TOUCH_PAD_COUNT; ++i) {
        if (is_button(i)) continue;

        uint32_t sum = 0;
        for (uint8_t s = 0; s < kCalibSamples; ++s) {
            sum += (uint16_t)touchRead(kGpio[i]);
            delayMicroseconds(200);
        }
        s_baseline [i] = (uint16_t)(sum / kCalibSamples);
        s_threshold[i] = apply_ratio(s_baseline[i]);

        if (s_baseline[i] < 20) {
            printf("[dd_touch] WARN: capacitive pad %u baseline=%u is too low — "
                   "check wire on GPIO%u\n", i, s_baseline[i], kGpio[i]);
            cap_wiring_ok = false;
        }
    }

    s_initialized = true;

    printf("[dd_touch] init OK — thumb cap baseline:%u threshold:%u  "
           "index/middle/ring: button (INPUT_PULLUP)  cap_wiring=%s\n",
           s_baseline[TOUCH_PAD_THUMB], s_threshold[TOUCH_PAD_THUMB],
           cap_wiring_ok ? "OK" : "CHECK WIRES");

    return AG_OK;
}

extern "C" ag_result_t dd_touch_read(touch_sample_t *out) {
    if (out == nullptr)  return AG_ERR_ARG;
    if (!s_initialized)  return AG_ERR_STATE;

    uint8_t mask = 0;
    for (uint8_t i = 0; i < TOUCH_PAD_COUNT; ++i) {
        if (is_button(i)) {
            /* Button pressed = GPIO pulled LOW by the switch. */
            const bool pressed = (digitalRead(kGpio[i]) == LOW);
            out->raw[i] = pressed ? 0u : 1u;   /* 0=pressed, 1=open — diagnostic only */
            if (pressed) {
                mask |= (uint8_t)(1u << i);
            }
        } else {
            /* Capacitive path (THUMB). */
            const uint16_t raw = (uint16_t)touchRead(kGpio[i]);
            out->raw[i] = raw;

            if (raw < s_threshold[i]) {
                mask |= (uint8_t)(1u << i);
                /* Do NOT update baseline while touched. */
            } else {
                const float bl = (1.0f - kEmaAlpha) * (float)s_baseline[i]
                               + kEmaAlpha           * (float)raw;
                s_baseline [i] = (uint16_t)bl;
                s_threshold[i] = apply_ratio(s_baseline[i]);
            }
        }
    }

    out->touched_mask = mask;
    out->t_us         = (uint64_t)esp_timer_get_time();
    return AG_OK;
}
