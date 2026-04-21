/* dd_touch.h — public interface of the ESP32 capacitive-touch driver.
 *
 * Layer: dd (device driver). May include Arduino/ESP-IDF touch headers.
 * Consumer is `srv_input`, which runs debounce + edge-detect FSM over
 * the stream of `touch_sample_t` values.
 *
 * Implementation lives in plan 03 (`docs/plans/03-dd-touch.md`).
 */

#ifndef DD_TOUCH_H
#define DD_TOUCH_H

#include "ag_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Initialise the four capacitive touch channels (thumb/index/middle/ring,
 * per `ag_pins.h`) and auto-calibrate a baseline per pad. Must be called
 * with the user's fingers NOT in contact with the pads so the baseline
 * reflects the idle capacitance.
 *
 * Not thread-safe; call once at boot. Returns AG_OK or AG_ERR_INIT. */
ag_result_t dd_touch_init(void);

/* Read all four pads into one timestamped sample. Non-blocking in
 * steady state (reads the peripheral's most recent filtered value).
 * `out->touched_mask` is already thresholded against the per-pad baseline.
 *
 * Thread-safety: NOT safe for concurrent callers — read from the dedicated
 * `t_touch` task. On error *out is left untouched. */
ag_result_t dd_touch_read(touch_sample_t *out);

#ifdef __cplusplus
}
#endif
#endif /* DD_TOUCH_H */
