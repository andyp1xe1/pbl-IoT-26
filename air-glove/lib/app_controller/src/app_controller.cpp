/* app_controller — top-level firmware orchestration (Phase I).
 *
 * Only this file (and tasks.cpp) may link FreeRTOS + the dd_* drivers.
 * `src/main.cpp` is stdio-only per NFR-MOD-001.
 */

#include <Arduino.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <freertos/timers.h>
#include <stdio.h>

#include "app_controller.h"
#include "tasks.h"

#include "dd_mpu6050.h"
#include "dd_touch.h"
#include "dd_ble_hid.h"
#include "srv_fusion.h"
#include "srv_motion.h"
#include "srv_input.h"

/* ── Queue handles (definitions) ───────────────────────────────────────── */
QueueHandle_t q_imu         = nullptr;
QueueHandle_t q_orientation = nullptr;
QueueHandle_t q_buttons     = nullptr;
QueueHandle_t q_hid         = nullptr;

/* ── Shared atomic state (definitions) ─────────────────────────────────── */
std::atomic<uint8_t> g_current_buttons{0};
std::atomic<int>     g_fsm_state{APP_STATE_INIT};

/* ── File-scope helpers ────────────────────────────────────────────────── */
namespace {

struct TaskEntry {
    const char  *name;
    TaskHandle_t handle;
};
static TaskEntry s_tasks[6] = {
    {"t_imu_sample", nullptr},
    {"t_fusion",     nullptr},
    {"t_touch",      nullptr},
    {"t_motion",     nullptr},
    {"t_app",        nullptr},
    {"t_ble_hid",    nullptr},
};

static TimerHandle_t s_heartbeat_timer = nullptr;

static const motion_config_t kDefaultMotionCfg = {
    /* deadzone_rad */ 0.006f,  /* ~0.35° — filters gyro noise (~0.002 rad)
                                 *  without blocking slow intentional tilts.  */
    /* gain_low     */ 600.0f,  /* linear term — gives ~25 px/frame at 30°/s  */
    /* gain_exp     */ 1.2f,    /* mild curve: fast flicks feel snappy         */
    /* velocity_cap */ 127.0f,  /* full int8 range                             */
    /* gain_y_scale */ 1.7f,    /* wrist roll (up/down) produces smaller deltas
                                 *  than pitch (left/right) for the same hand
                                 *  displacement — this levels them out.
                                 *  Tune up if Y is still slow, down if too fast. */
};

static const char *state_name(int s)
{
    switch (s) {
        case APP_STATE_INIT:    return "INIT";
        case APP_STATE_PAIRING: return "PAIRING";
        case APP_STATE_ACTIVE:  return "ACTIVE";
        default:                return "UNKNOWN";
    }
}

/* 1 Hz printf of each task's stack high-water-mark (in words) plus the
 * current top-level FSM state. Referenced in the E09 acceptance criterion
 * "no task high-water exceeds 75 % of allocated after 10-minute stress". */
static void heartbeat_cb(TimerHandle_t /*xTimer*/)
{
    const bool connected = dd_ble_hid_is_connected();
    printf("[heartbeat] state=%-7s  BLE=%s\n",
           state_name(g_fsm_state.load()),
           connected ? "connected" : "waiting for host");
    for (auto &t : s_tasks) {
        if (t.handle != nullptr) {
            const unsigned hwm =
                (unsigned)uxTaskGetStackHighWaterMark(t.handle);
            printf("[heartbeat]   %-14s stack free: %4u words\n", t.name, hwm);
        }
    }
}

[[noreturn]] static void fatal_init(const char *stage, ag_result_t rc)
{
    printf("[FATAL] %s failed rc=%d; restarting in 5 s\n", stage, rc);
    vTaskDelay(pdMS_TO_TICKS(5000));
    esp_restart();
    while (true) {}   /* unreachable */
}

} /* namespace */

/* ── Public entry point ────────────────────────────────────────────────── */

extern "C" ag_result_t app_controller_start(void)
{
    g_fsm_state.store(APP_STATE_INIT);

    /* ── 1. Drivers (fail-fast; log then reboot on error) ───────────── */
    printf("[app_controller] init stage 1: dd_mpu6050\n");
    ag_result_t rc = dd_mpu6050_init();
    if (rc != AG_OK) fatal_init("dd_mpu6050_init", rc);

    printf("[app_controller] init stage 2: dd_touch\n");
    rc = dd_touch_init();
    if (rc != AG_OK) fatal_init("dd_touch_init", rc);

    printf("[app_controller] init stage 3: dd_ble_hid\n");
    rc = dd_ble_hid_init("AirGlove");
    if (rc != AG_OK) fatal_init("dd_ble_hid_init", rc);

    /* ── 2. Services (cannot fail on valid inputs) ──────────────────── */
    printf("[app_controller] init services\n");
    /* beta=0.05: Madgwick's recommended base is 0.033 for IMU-only; 0.05 gives
     * a small extra margin against gyro bias drift without the "sticky /
     * fighting-back" feel that 0.15 caused during slow tilts. The motion-aware
     * guard in srv_fusion already suppresses accel correction during fast
     * movements, so beta only matters in the near-static regime. */
    (void)srv_fusion_init(0.05f);
    (void)srv_motion_init(&kDefaultMotionCfg);
    (void)srv_input_init(15);

    /* ── 3. Queues ─────────────────────────────────────────────────── */
    printf("[app_controller] create queues\n");
    q_imu         = xQueueCreate(4, sizeof(imu_sample_t));
    q_orientation = xQueueCreate(2, sizeof(oriented_frame_t));
    q_buttons     = xQueueCreate(8, sizeof(input_event_t));
    q_hid         = xQueueCreate(8, sizeof(hid_mouse_report_t));
    if (!q_imu || !q_orientation || !q_buttons || !q_hid) {
        fatal_init("queue_alloc", AG_ERR_INIT);
    }

    /* ── 4. Tasks (pinned per architecture.md §4.1) ─────────────────── *
     * Stack sizes below are in BYTES (ESP-IDF convention: StackType_t = 1 B). */
    printf("[app_controller] create tasks\n");

    xTaskCreatePinnedToCore(t_imu_sample_fn, "t_imu_sample",
                            2048, nullptr, 5, &s_tasks[0].handle, 0);
    xTaskCreatePinnedToCore(t_fusion_fn,     "t_fusion",
                            4096, nullptr, 4, &s_tasks[1].handle, 0);
    xTaskCreatePinnedToCore(t_touch_fn,      "t_touch",
                            2048, nullptr, 3, &s_tasks[2].handle, 0);
    xTaskCreatePinnedToCore(t_motion_fn,     "t_motion",
                            4096, nullptr, 3, &s_tasks[3].handle, 1);
    xTaskCreatePinnedToCore(t_app_fn,        "t_app",
                            3072, nullptr, 3, &s_tasks[4].handle, 1);
    xTaskCreatePinnedToCore(t_ble_hid_fn,    "t_ble_hid",
                            4096, nullptr, 6, &s_tasks[5].handle, 1);

    for (auto &t : s_tasks) {
        if (t.handle == nullptr) fatal_init("task_create", AG_ERR_INIT);
    }

    /* ── 5. Heartbeat timer (1 Hz stack HWM + FSM log) ──────────────── */
    s_heartbeat_timer = xTimerCreate(
        "heartbeat", pdMS_TO_TICKS(1000), pdTRUE, nullptr, heartbeat_cb);
    if (s_heartbeat_timer != nullptr) {
        xTimerStart(s_heartbeat_timer, 0);
    }

    /* INIT complete — t_ble_hid will switch to PAIRING on its first tick. */
    printf("[app_controller] up\n");
    return AG_OK;
}
