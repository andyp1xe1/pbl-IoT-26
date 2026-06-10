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

/* Per-pad capacitive baseline (counts). Button pads carry a meaningless 0.
 * Setter applies immediately and recomputes thresholds; save persists to NVS
 * so the next boot starts from the calibrated value instead of the bench
 * sample. EMA self-recalibration keeps running on top regardless. */
void        dd_touch_set_baselines (const uint16_t bl[TOUCH_PAD_COUNT]);
ag_result_t dd_touch_save_baselines(void);

#ifdef __cplusplus
}
#endif
#endif /* DD_TOUCH_H */
