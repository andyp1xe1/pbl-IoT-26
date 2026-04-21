#ifndef SRV_INPUT_H
#define SRV_INPUT_H

#include "ag_types.h"
#include "dd_touch.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    INPUT_EVT_NONE = 0,
    INPUT_EVT_PRESS,
    INPUT_EVT_RELEASE
} input_evt_kind_t;

typedef struct {
    touch_pad_id_t    pad;
    input_evt_kind_t  kind;
    uint64_t          t_us;
} input_event_t;

ag_result_t srv_input_init(uint16_t debounce_ms);

/* Push one touch sample through the FSM; pop 0..N events into caller's buffer.
 * Never blocks. `*out_len` is set to the number of events written. */
ag_result_t srv_input_process(const touch_sample_t *s,
                              input_event_t *out, size_t out_cap, size_t *out_len);

void srv_input_reset(void);

#ifdef __cplusplus
}
#endif
#endif /* SRV_INPUT_H */
