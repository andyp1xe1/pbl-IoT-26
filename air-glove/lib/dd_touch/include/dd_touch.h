#ifndef DD_TOUCH_H
#define DD_TOUCH_H

#include "ag_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Initialise the four capacitive touch channels and auto-calibrate a
 * baseline per pad (assumes fingers are not in contact at init). */
ag_result_t dd_touch_init(void);

/* Read all four pads into one sample. `touched_mask` reflects current
 * thresholded state. */
ag_result_t dd_touch_read(touch_sample_t *out);

#ifdef __cplusplus
}
#endif
#endif /* DD_TOUCH_H */
