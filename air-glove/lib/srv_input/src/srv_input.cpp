/* srv_input — stub until plan 07 lands. Pure C++; no Arduino/ESP-IDF. */

#include "srv_input.h"

extern "C" ag_result_t srv_input_init(uint16_t debounce_ms) {
    (void)debounce_ms;
    return AG_OK;
}

extern "C" ag_result_t srv_input_process(const touch_sample_t *s,
                                         input_event_t *out, size_t out_cap,
                                         size_t *out_len) {
    (void)s; (void)out; (void)out_cap;
    if (!out_len) return AG_ERR_ARG;
    *out_len = 0;
    return AG_OK;
}

extern "C" void srv_input_reset(void) {}
