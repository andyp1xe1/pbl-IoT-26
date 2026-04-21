/* ag_types.h — shared POD types and result codes for Air Glove.
 *
 * This header is the layer-crossing vocabulary: every `dd_*`, `srv_*`,
 * and `app_*` lib speaks in these types. It deliberately depends on
 * the C stdlib only — no Arduino, no ESP-IDF — so it compiles in
 * both `env:esp32dev` and `env:native` (host unit tests).
 *
 * Owner: E02 (HW Abstraction Layer).
 */

#ifndef AG_TYPES_H
#define AG_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ----- Result codes --------------------------------------------------- */

/* Unified result type returned by every public function in the lib tree.
 * AG_OK (zero) is success; any negative value is an error. Callers may
 * treat the numeric value as a hint for logging but MUST NOT rely on a
 * specific non-OK code being returned for a given failure (except where
 * an individual lib's header documents one explicitly). */
typedef int ag_result_t;

#define AG_OK          0    /* Success.                                  */
#define AG_ERR_IO     -1    /* Low-level bus / transport failure.        */
#define AG_ERR_INIT   -2    /* Device / lib not initialised, or init failed. */
#define AG_ERR_ARG    -3    /* Caller-supplied argument invalid (NULL, out of range). */
#define AG_ERR_STATE  -4    /* Called at the wrong time (e.g. read before init). */

/* ----- IMU ------------------------------------------------------------ */

/* One 6-axis IMU sample in physical units.
 * Units: linear acceleration in m/s^2, angular rate in rad/s.
 * `t_us` is a monotonic microsecond stamp (esp_timer_get_time() on target,
 * an equivalent monotonic clock on native). Callers should treat the
 * struct as value-typed and never mutate fields after the producer fills it. */
typedef struct {
    float ax, ay, az;
    float gx, gy, gz;
    uint64_t t_us;
} imu_sample_t;

/* ----- Touch ---------------------------------------------------------- */

/* Logical identity of each fingertip pad.
 * THUMB is the common reference electrode; INDEX/MIDDLE/RING are the
 * switched electrodes used for clicks and (in Phase II) scroll. The
 * trailing TOUCH_PAD_COUNT is intentionally the array length, not a
 * valid pad id. */
typedef enum {
    TOUCH_PAD_THUMB = 0,
    TOUCH_PAD_INDEX,
    TOUCH_PAD_MIDDLE,
    TOUCH_PAD_RING,
    TOUCH_PAD_COUNT
} touch_pad_id_t;

/* One snapshot of all four capacitive pads under a single timestamp.
 * `raw[i]` is the pad's raw capacitance reading (device-specific scale,
 * produced by the ESP32 touch peripheral; lower == finger closer).
 * `touched_mask` packs the thresholded boolean state — bit N reflects
 * touch_pad_id_t N. Producer (dd_touch) writes atomically; consumers
 * (srv_input) read without locking. */
typedef struct {
    uint16_t raw[TOUCH_PAD_COUNT];
    uint8_t  touched_mask;
    uint64_t t_us;
} touch_sample_t;

/* ----- BLE HID mouse report ------------------------------------------ */

/* Wire-compatible payload for the BLE HID mouse input report.
 * `dx`/`dy` are signed 8-bit cursor deltas per HID spec (range -127..127).
 * `buttons` is a bitfield: bit 0 = left, bit 1 = right, bit 2 = middle.
 * `wheel` is signed 8-bit vertical scroll (Phase II). Ownership: the
 * producer task passes this by value through a FreeRTOS queue, so there
 * is no shared mutable aliasing. */
typedef struct {
    int8_t  dx;
    int8_t  dy;
    uint8_t buttons;
    int8_t  wheel;
} hid_mouse_report_t;

#ifdef __cplusplus
}
#endif
#endif /* AG_TYPES_H */
