#pragma once
/* Internal header for app_controller — NOT installed in include/.
 *
 * Declares the six task entry-point functions, the four shared queue
 * handles, the shared atomic button byte, and the POD that flows through
 * q_orientation. This header pulls in FreeRTOS types, so it must never
 * be exposed outside the lib.
 */

#include <atomic>
#include <stdint.h>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "ag_types.h"
#include "srv_fusion.h"   /* for quat_t */

/* POD that travels through q_orientation (core 0 → core 1). */
typedef struct {
    quat_t   q;
    uint64_t t_us;
} oriented_frame_t;

/* Queue handles — defined in app_controller.cpp, consumed in tasks.cpp. */
extern QueueHandle_t q_imu;           /* depth 4, item imu_sample_t       */
extern QueueHandle_t q_orientation;   /* depth 2, item oriented_frame_t   */
extern QueueHandle_t q_buttons;       /* depth 8, item input_event_t      */
extern QueueHandle_t q_hid;           /* depth 8, item hid_mouse_report_t */

/* Current mouse-button byte (bit 0 = L, bit 1 = R). Writer is t_app;
 * readers are t_motion (to stamp motion reports) and the heartbeat.
 * Single-byte atomic load/store is lock-free on ESP32. */
extern std::atomic<uint8_t> g_current_buttons;

/* True while any pad mapped to AG_CLICK_SCROLL_MODE is held.
 * Writer: t_app (via apply_action). Reader: t_motion (routes dy → wheel
 * instead of cursor). The pad that drives this is data-driven via the
 * companion-app config; nothing is hard-coded to the ring finger any more. */
extern std::atomic<bool> g_scroll_mode;

/* Opaque top-level FSM state for the heartbeat to log. */
enum app_state_t { APP_STATE_INIT = 0, APP_STATE_PAIRING, APP_STATE_ACTIVE };
extern std::atomic<int> g_fsm_state;

/* Soft-sleep flag. When true the device keeps its BLE link up (so the
 * companion app still sees it) but stops emitting HID mouse reports and
 * powers the MPU6050 down. Toggled by t_cfg in response to CMD_SLEEP /
 * CMD_WAKE from the companion. Producers (t_imu_sample, t_touch) and
 * sinks (t_app, t_motion) check this each iteration and short-circuit
 * when set. Single-byte atomic is lock-free on ESP32. */
extern std::atomic<bool> g_sleeping;

/* Telemetry publish period in milliseconds. Adjusted by t_cfg in response
 * to CMD_TELE_IDLE / NORMAL / FAST so the BLE airtime matches what the
 * companion is actually watching. Default 100 ms (10 Hz). */
extern std::atomic<uint16_t> g_telemetry_period_ms;

/* ── Latest-sample telemetry snapshot (for the companion-app GATT service) ──
 * Writers: t_imu_sample (IMU axes), t_touch (pad raws). Reader: t_cfg.
 * Each is a single 16-bit word — lock-free on ESP32. Values are pre-scaled to
 * the wire units the companion app expects (milli-g, milli-deg/s, raw counts). */
extern std::atomic<int16_t>  g_tele_accel_mg[3];
extern std::atomic<int16_t>  g_tele_gyro_mdps[3];
extern std::atomic<uint16_t> g_tele_touch_raw[4];

/* Gyro-bias calibration shared state.
 *
 *  - g_gyro_cal_running: t_cfg flips this true to start sample accumulation
 *    and false to stop. Single-writer (t_cfg), single-reader (t_imu_sample).
 *  - g_gyro_cal_count: incremented by t_imu_sample per accepted sample. t_cfg
 *    polls it to know progress. Wraps on uint32 — calibrations sample 300.
 *  - g_gyro_cal_sum_*: accumulator in body-frame milli-rad/s × 1024 (fixed-
 *    point i64). Lock-free: only t_imu_sample writes, and only when running;
 *    t_cfg reads after stopping it. Holds plenty of headroom: 300 samples ×
 *    32 rad/s × 1024 × 1000 ≈ 1e10, well below INT64_MAX. */
extern std::atomic<bool>     g_gyro_cal_running;
extern std::atomic<uint32_t> g_gyro_cal_count;
extern std::atomic<int64_t>  g_gyro_cal_sum[3];

/* Touch-baseline calibration. Same shape as the gyro path: t_cfg flips the
 * running flag, t_touch accumulates raw cap-pad reads at its native 100 Hz,
 * t_cfg polls count for progress and computes baseline = sum/count when done.
 * Buttons contribute nothing (their raw is 0/4095 — meaningless as a baseline)
 * so accumulation runs only on capacitive pads. */
extern std::atomic<bool>     g_touch_cal_running;
extern std::atomic<uint32_t> g_touch_cal_count;
extern std::atomic<uint32_t> g_touch_cal_sum[4];   /* indexed by touch_pad_id_t */

/* Task entry points. */
void t_imu_sample_fn(void *);
void t_fusion_fn    (void *);
void t_touch_fn     (void *);
void t_motion_fn    (void *);
void t_app_fn       (void *);
void t_ble_hid_fn   (void *);
void t_cfg_fn       (void *);
