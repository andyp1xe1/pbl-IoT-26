#ifndef DD_TOUCH_H
#define DD_TOUCH_H

#include "ag_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Initialise the four capacitive touch channels and auto-calibrate a
 * baseline per pad (assumes fingers are not in contact at init). */
ag_result_t dd_touch_init(void);

/* Re-run the capacitive baseline calibration at runtime.  Call when the
 * glove is flat on a table with no finger contact.  Thread-safety: the
 * brief window during which s_threshold is being updated is benign — the
 * worst case is one missed or extra touch detection on t_touch's next tick.
 * No-op (returns AG_ERR_STATE) if dd_touch_init() has not been called. */
ag_result_t dd_touch_recalibrate(void);

/* Read all four pads into one sample. `touched_mask` reflects current
 * thresholded state. */
ag_result_t dd_touch_read(touch_sample_t *out);

#ifdef __cplusplus
}
#endif
#endif /* DD_TOUCH_H */
