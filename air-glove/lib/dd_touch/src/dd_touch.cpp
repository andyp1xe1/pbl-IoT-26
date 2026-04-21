/* dd_touch — stub until plan 03 lands. */

#include "dd_touch.h"

extern "C" ag_result_t dd_touch_init(void) {
    return AG_ERR_INIT;
}

extern "C" ag_result_t dd_touch_read(touch_sample_t *out) {
    (void)out;
    return AG_ERR_STATE;
}
