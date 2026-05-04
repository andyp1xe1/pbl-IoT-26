/* dd_touch — ESP32 native capacitive-touch driver (Phase I implementation).
 *
 * Uses the Arduino-ESP32 `touchRead()` wrapper. A future swap to the
 * ESP-IDF `driver/touch_sensor.h` (native, interrupt-capable, non-blocking)
 * is tracked as a follow-up under E14 (power management).
 *
 * ADR-003 locks in the ESP32 native peripheral as our sensing method,
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

/* GPIO per `touch_pad_id_t` — indices MUST align with the enum:
 *   THUMB=0 → T0 (GPIO4), INDEX=1 → T6 (GPIO14),
 *   MIDDLE=2 → T3 (GPIO15), RING=3 → T4 (GPIO13).
 *
 * INDEX was moved from GPIO2 (T2) to GPIO14 (T6): GPIO2 has a 10 kΩ
 * LED pulldown on all standard ESP32 dev boards that causes touchRead()
 * to return 0 regardless of finger contact. GPIO14 is load-free. */
static const uint8_t kGpio[TOUCH_PAD_COUNT] = { 4, 14, 15, 13 };

/* Touch fires when raw < baseline * kThreshRatio.
 * 0.85 = 15% capacitance drop needed — works with bare wire ends on all
 * strapping-load-free GPIOs (GPIO4, GPIO14, GPIO15, GPIO13). */
static constexpr float kThreshRatio = 0.85f;
static constexpr float   kEmaAlpha     = 0.05f;  /* ~0.2 s baseline drift adapt       */
static constexpr uint8_t kCalibSamples = 50;

static uint16_t s_baseline [TOUCH_PAD_COUNT];
static uint16_t s_threshold[TOUCH_PAD_COUNT];
static bool     s_initialized = false;

static inline uint16_t apply_ratio(uint16_t b) {
    return (uint16_t)((float)b * kThreshRatio);
}

} /* namespace */

extern "C" ag_result_t dd_touch_init(void) {
    /* Prime the peripheral: the very first reading after boot is often 0. */
    for (uint8_t i = 0; i < TOUCH_PAD_COUNT; ++i) {
        (void)touchRead(kGpio[i]);
    }
    delay(10);

    for (uint8_t i = 0; i < TOUCH_PAD_COUNT; ++i) {
        uint32_t sum = 0;
        for (uint8_t s = 0; s < kCalibSamples; ++s) {
            sum += (uint16_t)touchRead(kGpio[i]);
            delayMicroseconds(200);
        }
        s_baseline [i] = (uint16_t)(sum / kCalibSamples);
        s_threshold[i] = apply_ratio(s_baseline[i]);
    }

    s_initialized = true;

    bool wiring_ok = true;
    for (uint8_t i = 0; i < TOUCH_PAD_COUNT; ++i) {
        if (s_baseline[i] < 20) {
            printf("[dd_touch] WARN: pad %u baseline=%u is too low — "
                   "check wire on GPIO%u is not shorted or disconnected\n",
                   i, s_baseline[i], kGpio[i]);
            wiring_ok = false;
        }
    }
    printf("[dd_touch] baselines — thumb:%u  index:%u  middle:%u  ring:%u  "
           "(thresholds: %u  %u  %u  %u)  wiring=%s\n",
           s_baseline[0], s_baseline[1], s_baseline[2], s_baseline[3],
           s_threshold[0], s_threshold[1], s_threshold[2], s_threshold[3],
           wiring_ok ? "OK" : "CHECK WIRES");
    if (!wiring_ok) {
        printf("[dd_touch] Normal baseline is 300-1500. Low values mean the\n"
               "           wire is not in the breadboard row for that GPIO.\n"
               "           Wires must be free in air during boot.\n");
    }
    return AG_OK;
}

extern "C" ag_result_t dd_touch_read(touch_sample_t *out) {
    if (out == nullptr)  return AG_ERR_ARG;
    if (!s_initialized)  return AG_ERR_STATE;

    uint8_t mask = 0;
    for (uint8_t i = 0; i < TOUCH_PAD_COUNT; ++i) {
        const uint16_t raw = (uint16_t)touchRead(kGpio[i]);
        out->raw[i] = raw;

        if (raw < s_threshold[i]) {
            mask |= (uint8_t)(1U << i);
            /* Intentionally do NOT update baseline while touched — otherwise
             * a held touch would slowly redefine "idle" as "low". */
        } else {
            const float bl = (1.0f - kEmaAlpha) * (float)s_baseline[i]
                           + kEmaAlpha           * (float)raw;
            s_baseline [i] = (uint16_t)bl;
            s_threshold[i] = apply_ratio(s_baseline[i]);
        }
    }

    out->touched_mask = mask;
    out->t_us         = (uint64_t)esp_timer_get_time();
    return AG_OK;
}
