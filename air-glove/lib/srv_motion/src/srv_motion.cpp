/* srv_motion — stub until plan 06 lands. Pure C++; no Arduino/ESP-IDF. */

#include "srv_motion.h"

extern "C" ag_result_t srv_motion_init(const motion_config_t *cfg) {
    (void)cfg;
    return AG_OK;
}

extern "C" ag_result_t srv_motion_update(const quat_t *q, float dt_s,
                                         int8_t *dx, int8_t *dy) {
    (void)q; (void)dt_s;
    if (!dx || !dy) return AG_ERR_ARG;
    *dx = 0;
    *dy = 0;
    return AG_OK;
}

extern "C" void srv_motion_set_clutch(bool active) { (void)active; }
extern "C" void srv_motion_reset(void) {}
