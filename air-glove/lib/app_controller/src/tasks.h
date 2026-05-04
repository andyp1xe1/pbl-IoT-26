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

/* True while the ring finger is held and scroll mode is active.
 * Writer: t_app. Reader: t_motion (routes dy → wheel instead of cursor). */
extern std::atomic<bool> g_scroll_mode;

/* Opaque top-level FSM state for the heartbeat to log. */
enum app_state_t { APP_STATE_INIT = 0, APP_STATE_PAIRING, APP_STATE_ACTIVE };
extern std::atomic<int> g_fsm_state;

/* Task entry points. */
void t_imu_sample_fn(void *);
void t_fusion_fn    (void *);
void t_touch_fn     (void *);
void t_motion_fn    (void *);
void t_app_fn       (void *);
void t_ble_hid_fn   (void *);
