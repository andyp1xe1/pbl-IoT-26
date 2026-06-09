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
#include "dd_ble_cfg.h"
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
std::atomic<bool>    g_scroll_mode{false};
std::atomic<bool>    g_sleeping{false};
std::atomic<uint16_t> g_telemetry_period_ms{50};   /* 20 Hz default */

/* ── Telemetry snapshot (definitions; declared in tasks.h) ─────────────── */
std::atomic<int16_t>  g_tele_accel_mg[3]  = {};
std::atomic<int16_t>  g_tele_gyro_mdps[3] = {};
std::atomic<uint16_t> g_tele_touch_raw[4] = {};

/* ── File-scope helpers ────────────────────────────────────────────────── */
namespace {

struct TaskEntry {
    const char  *name;
    TaskHandle_t handle;
};
static TaskEntry s_tasks[7] = {
    {"t_imu_sample", nullptr},
    {"t_fusion",     nullptr},
    {"t_touch",      nullptr},
    {"t_motion",     nullptr},
    {"t_app",        nullptr},
    {"t_ble_hid",    nullptr},
    {"t_cfg",        nullptr},
};

static TimerHandle_t s_heartbeat_timer = nullptr;

/* Boot-time motion mapping: equivalent to the dd_ble_cfg built-in defaults
 * (pitch+yaw → dx, roll → dy with 1.7× boost), so cursor behaviour at the
 * "Air Glove just powered on" instant matches what the companion will read
 * back from NVS once t_motion picks up the persisted config. */
static const motion_config_t kDefaultMotionCfg = {
    /* mix_x_milli  */ { 0, 0, 0, 0, 0, 0,    0, +1000, -1000 },
    /* mix_y_milli  */ { 0, 0, 0, 0, 0, 0, -1700,     0,     0 },
    /* sens_x_milli */ 1000,
    /* sens_y_milli */ 1000,
    /* deadzone_rad */ 0.004f,
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

/* Heartbeat: single-line liveness + lazy stack alarm. One [heartbeat] line
 * every tick (5 s) shows FSM + BLE state at a glance. The per-task stack
 * high-water-marks were previously logged unconditionally (8 lines / tick),
 * which drowned out everything else; now we only spell them out when at
 * least one task drops below 200 words free — the case the E09 acceptance
 * criterion actually cares about. At healthy steady state this stays at
 * one line per tick. */
static constexpr unsigned kStackAlarmWords = 200;

static void heartbeat_cb(TimerHandle_t /*xTimer*/)
{
    const bool connected = dd_ble_hid_is_connected();

    bool stack_alarm = false;
    for (auto &t : s_tasks) {
        if (t.handle != nullptr) {
            unsigned hwm = (unsigned)uxTaskGetStackHighWaterMark(t.handle);
            if (hwm < kStackAlarmWords) { stack_alarm = true; break; }
        }
    }

    printf("[heartbeat] state=%-7s  BLE=%s%s\n",
           state_name(g_fsm_state.load()),
           connected ? "connected" : "waiting for host",
           stack_alarm ? "  STACK LOW (details below)" : "");

    if (stack_alarm) {
        for (auto &t : s_tasks) {
            if (t.handle != nullptr) {
                const unsigned hwm =
                    (unsigned)uxTaskGetStackHighWaterMark(t.handle);
                printf("[heartbeat]   %-14s stack free: %4u words%s\n",
                       t.name, hwm,
                       hwm < kStackAlarmWords ? "  <-- low" : "");
            }
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

    /* BLE two-phase init: bring up the server and register every service
     * BEFORE the att table is finalised. Adding a service after
     * dd_ble_hid_start() leaves its characteristics out of the att table
     * on NimBLE-Arduino 1.x — BlueZ discovers the service with zero chars
     * and Web Bluetooth fails at getCharacteristic. */
    printf("[app_controller] init stage 3a: dd_ble_hid (server)\n");
    rc = dd_ble_hid_init_server("AirGlove");
    if (rc != AG_OK) fatal_init("dd_ble_hid_init_server", rc);

    /* Companion-app config/telemetry service shares the NimBLE server created
     * above. Non-fatal: if it fails the glove still works as a plain mouse. */
    printf("[app_controller] init stage 3b: dd_ble_cfg\n");
    rc = dd_ble_cfg_init(nullptr);
    if (rc != AG_OK) {
        printf("[app_controller] WARN dd_ble_cfg_init rc=%d — continuing without "
               "companion service\n", rc);
    }

    printf("[app_controller] init stage 3c: dd_ble_hid (start + advertise)\n");
    rc = dd_ble_hid_start();
    if (rc != AG_OK) fatal_init("dd_ble_hid_start", rc);

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
                            3072, nullptr, 3, &s_tasks[2].handle, 0);
    xTaskCreatePinnedToCore(t_motion_fn,     "t_motion",
                            4096, nullptr, 3, &s_tasks[3].handle, 1);
    xTaskCreatePinnedToCore(t_app_fn,        "t_app",
                            3072, nullptr, 3, &s_tasks[4].handle, 1);
    xTaskCreatePinnedToCore(t_ble_hid_fn,    "t_ble_hid",
                            4096, nullptr, 6, &s_tasks[5].handle, 1);
    /* Low priority: companion telemetry/config is best-effort, must never
     * starve the motion or HID path. Larger stack covers NVS (flash) writes. */
    xTaskCreatePinnedToCore(t_cfg_fn,        "t_cfg",
                            4096, nullptr, 2, &s_tasks[6].handle, 1);

    for (auto &t : s_tasks) {
        if (t.handle == nullptr) fatal_init("task_create", AG_ERR_INIT);
    }

    /* ── 5. Heartbeat timer (1 Hz stack HWM + FSM log) ──────────────── */
    s_heartbeat_timer = xTimerCreate(
        "heartbeat", pdMS_TO_TICKS(5000), pdTRUE, nullptr, heartbeat_cb);
    if (s_heartbeat_timer != nullptr) {
        xTimerStart(s_heartbeat_timer, 0);
    }

    /* INIT complete — t_ble_hid will switch to PAIRING on its first tick. */
    printf("[app_controller] up\n");
    return AG_OK;
}
