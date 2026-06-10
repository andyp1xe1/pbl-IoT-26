#ifndef AG_TYPES_H
#define AG_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int ag_result_t;
#define AG_OK          0
#define AG_ERR_IO     -1
#define AG_ERR_INIT   -2
#define AG_ERR_ARG    -3
#define AG_ERR_STATE  -4

/* One 6-axis IMU sample in physical units. */
typedef struct {
    float ax, ay, az;   /* m/s^2 */
    float gx, gy, gz;   /* rad/s */
    uint64_t t_us;      /* monotonic microseconds */
} imu_sample_t;

/* Slot order is hardware wiring, not anatomical. Renumbering is a wire-format
 * break (BLE codec, NVS baselines, companion config). */
typedef enum {
    TOUCH_PAD_PINKY = 0,
    TOUCH_PAD_INDEX,
    TOUCH_PAD_RING,
    TOUCH_PAD_MIDDLE,
    TOUCH_PAD_COUNT
} touch_pad_id_t;

typedef struct {
    uint16_t raw[TOUCH_PAD_COUNT];
    uint8_t  touched_mask;   /* bit N = pad N is currently below its threshold */
    uint64_t t_us;
} touch_sample_t;

/* BLE HID mouse report, wire-compatible with the descriptor in dd_ble_hid. */
typedef struct {
    int8_t  dx;
    int8_t  dy;
    uint8_t buttons;   /* bit 0 = L, bit 1 = R, bit 2 = M */
    int8_t  wheel;
} hid_mouse_report_t;

/* Motion-mix input axes — cursor-eligible signals only. Order is wire-stable;
 * extending it requires a config schema bump. The Y-axis lanes (GY, AY, PITCH)
 * are intentionally absent: rotation about glove Y is wrist twist, which is
 * posture, not gesture, and is decoupled from cursor motion entirely.
 * Wrist-twist data still flows through Madgwick (all 6 raw axes are required
 * to compute q) and the wrist-roll compensation step extracts it from q,
 * but it never reaches the cursor mix matrix. */
typedef enum {
    AG_MIX_GX = 0,   /* raw gyro X (rad/s)  — pitch-gesture feed-forward       */
    AG_MIX_GZ,       /* raw gyro Z          — yaw-gesture feed-forward         */
    AG_MIX_AX,       /* raw accel X (m/s²)  — spike-style only                 */
    AG_MIX_AZ,       /* raw accel Z         — spike-style only                 */
    AG_MIX_ROLL,     /* compensated rotation about world X per frame (rad)     */
    AG_MIX_YAW,      /* compensated rotation about world Z per frame (rad)     */
    AG_MIX_COUNT
} ag_mix_axis_t;

#ifdef __cplusplus
}
#endif
#endif /* AG_TYPES_H */
