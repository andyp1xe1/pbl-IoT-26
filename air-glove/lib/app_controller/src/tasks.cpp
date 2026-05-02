/* tasks.cpp — FreeRTOS task bodies for app_controller.
 *
 * See docs/srs/architecture.md §4 for the task table, §6 for the data
 * flow, and docs/plans/08-app-controller.md for per-task rationale.
 */

#include "tasks.h"

#include <stdio.h>
#include <stdint.h>
#include <math.h>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "dd_mpu6050.h"
#include "dd_touch.h"
#include "dd_ble_hid.h"
#include "srv_fusion.h"
#include "srv_motion.h"
#include "srv_input.h"

namespace {

/* Drop-oldest queue enqueue: if the queue is full, pop one item then
 * enqueue. Safe here because every queue has a single-task producer,
 * so there is no intra-producer race inside this task's context. */
template <typename T>
static inline void queue_put_drop_oldest(QueueHandle_t q, const T *item)
{
    if (xQueueSend(q, item, 0) != pdTRUE) {
        T trash;
        (void)xQueueReceive(q, &trash, 0);
        (void)xQueueSend(q, item, 0);
    }
}

static inline int8_t sat_add_i8(int a, int b)
{
    int r = a + b;
    if (r >  127) r =  127;
    if (r < -127) r = -127;
    return (int8_t)r;
}

} /* namespace */

/* ── t_imu_sample — poll MPU6050 at 100 Hz ─────────────────────────────── */
void t_imu_sample_fn(void *)
{
    TickType_t        last   = xTaskGetTickCount();
    const TickType_t  period = pdMS_TO_TICKS(10);
    uint32_t          count  = 0;

    for (;;) {
        imu_sample_t s;
        ag_result_t rc = dd_mpu6050_read(&s);
        if (rc == AG_OK) {
            queue_put_drop_oldest(q_imu, &s);
            /* Log actual sensor values once per second (every 100 reads). */
            if (++count % 100 == 0) {
                printf("[imu] accel=[%+5.2f %+5.2f %+5.2f] m/s²  "
                       "gyro=[%+6.3f %+6.3f %+6.3f] rad/s\n",
                       (double)s.ax, (double)s.ay, (double)s.az,
                       (double)s.gx, (double)s.gy, (double)s.gz);
            }
        } else {
            printf("[imu] read error rc=%d\n", rc);
        }
        vTaskDelayUntil(&last, period);
    }
}

/* ── t_fusion — q_imu → Madgwick → q_orientation ──────────────────────── */
void t_fusion_fn(void *)
{
    uint32_t count = 0;

    for (;;) {
        imu_sample_t s;
        if (xQueueReceive(q_imu, &s, portMAX_DELAY) != pdTRUE) continue;

        oriented_frame_t f;
        if (srv_fusion_update(&s, &f.q) != AG_OK) continue;
        f.t_us = s.t_us;
        queue_put_drop_oldest(q_orientation, &f);

        /* Log Euler angles once per second (every 100 fused frames). */
        if (++count % 100 == 0) {
            const quat_t &q = f.q;
            const float kRad2Deg = 57.2957795f;
            float roll  = atan2f(2.0f*(q.q0*q.q1 + q.q2*q.q3),
                                 1.0f - 2.0f*(q.q1*q.q1 + q.q2*q.q2)) * kRad2Deg;
            float pitch = asinf( 2.0f*(q.q0*q.q2 - q.q3*q.q1))        * kRad2Deg;
            float yaw   = atan2f(2.0f*(q.q0*q.q3 + q.q1*q.q2),
                                 1.0f - 2.0f*(q.q2*q.q2 + q.q3*q.q3)) * kRad2Deg;
            printf("[fusion] roll=%+6.1f  pitch=%+6.1f  yaw=%+6.1f  deg\n",
                   (double)roll, (double)pitch, (double)yaw);
        }
    }
}

/* ── t_touch — poll pads at 100 Hz → q_buttons ────────────────────────── */
void t_touch_fn(void *)
{
    TickType_t        last   = xTaskGetTickCount();
    const TickType_t  period = pdMS_TO_TICKS(10);

    for (;;) {
        touch_sample_t s;
        if (dd_touch_read(&s) == AG_OK) {
            input_event_t evts[TOUCH_PAD_COUNT];
            size_t n = 0;
            if (srv_input_process(&s, evts, TOUCH_PAD_COUNT, &n) == AG_OK) {
                for (size_t i = 0; i < n; ++i) {
                    queue_put_drop_oldest(q_buttons, &evts[i]);
                }
            }
        }
        vTaskDelayUntil(&last, period);
    }
}

/* ── t_motion — q_orientation → dx/dy → q_hid ─────────────────────────── */
void t_motion_fn(void *)
{
    uint64_t last_t_us = 0;

    for (;;) {
        oriented_frame_t f;
        if (xQueueReceive(q_orientation, &f, portMAX_DELAY) != pdTRUE) continue;

        float dt_s = 0.01f;
        if (last_t_us != 0 && f.t_us > last_t_us) {
            dt_s = (float)(f.t_us - last_t_us) * 1e-6f;
        }
        last_t_us = f.t_us;

        int8_t dx = 0, dy = 0;
        if (srv_motion_update(&f.q, dt_s, &dx, &dy) != AG_OK) continue;

        /* Only emit motion reports when the cursor actually moves.
         * Button-only changes go through t_app directly. Each report
         * carries the CURRENT button byte so t_ble_hid can coalesce
         * without clobbering button state. */
        if (dx != 0 || dy != 0) {
            hid_mouse_report_t r;
            r.dx      = dx;
            r.dy      = dy;
            r.buttons = g_current_buttons.load();
            r.wheel   = 0;
            queue_put_drop_oldest(q_hid, &r);
        }
    }
}

/* ── t_app — drain button events, publish on change ───────────────────── */
void t_app_fn(void *)
{
    for (;;) {
        input_event_t ev;
        if (xQueueReceive(q_buttons, &ev, portMAX_DELAY) != pdTRUE) continue;

        /* Map fingertip pad → HID button bit. Thumb is the common
         * return electrode and is never mapped. Ring is reserved for
         * Phase II (E10 scroll/clutch). */
        uint8_t bit = 0;
        const char *btn_name = nullptr;
        if      (ev.pad == TOUCH_PAD_INDEX)  { bit = 0x01; btn_name = "LEFT (index)";  }
        else if (ev.pad == TOUCH_PAD_MIDDLE) { bit = 0x02; btn_name = "RIGHT (middle)"; }
        else continue;

        uint8_t prev = g_current_buttons.load();
        uint8_t next = prev;
        if      (ev.kind == INPUT_EVT_PRESS)   next |=  bit;
        else if (ev.kind == INPUT_EVT_RELEASE) next &= ~bit;
        else continue;

        if (next != prev) {
            g_current_buttons.store(next);
            printf("[touch] %s click %s\n",
                   btn_name,
                   (ev.kind == INPUT_EVT_PRESS) ? "PRESSED" : "released");
            hid_mouse_report_t r;
            r.dx      = 0;
            r.dy      = 0;
            r.buttons = next;
            r.wheel   = 0;
            queue_put_drop_oldest(q_hid, &r);
        }
    }
}

/* ── t_ble_hid — owns FSM, drains q_hid, sends at ≤ 125 Hz ────────────── */
void t_ble_hid_fn(void *)
{
    g_fsm_state.store(APP_STATE_PAIRING);
    printf("[ble] PAIRING — advertising as AirGlove, open Bluetooth settings on your host\n");

    bool was_connected = false;

    for (;;) {
        if (!dd_ble_hid_is_connected()) {
            if (was_connected) {
                printf("[ble] host disconnected — back to PAIRING, re-advertising\n");
                was_connected = false;
            }
            g_fsm_state.store(APP_STATE_PAIRING);
            /* Drain so upstream producers don't stall while we wait. */
            hid_mouse_report_t trash;
            while (xQueueReceive(q_hid, &trash, 0) == pdTRUE) { }
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        if (!was_connected) {
            printf("[ble] ACTIVE — host connected, mouse reports flowing at up to 125 Hz\n");
            was_connected = true;
        }
        g_fsm_state.store(APP_STATE_ACTIVE);

        hid_mouse_report_t merged;
        if (xQueueReceive(q_hid, &merged, pdMS_TO_TICKS(8)) != pdTRUE) {
            /* No report in the last 8 ms (~125 Hz pacing). Loop back to
             * re-check the connection state. */
            continue;
        }

        /* Coalesce up to 3 additional reports waiting in q_hid: sum
         * dx/dy (int8-saturating), keep the latest buttons/wheel. */
        hid_mouse_report_t extra;
        int drained = 0;
        while (drained < 3 &&
               xQueueReceive(q_hid, &extra, 0) == pdTRUE) {
            merged.dx      = sat_add_i8(merged.dx,    extra.dx);
            merged.dy      = sat_add_i8(merged.dy,    extra.dy);
            merged.wheel   = sat_add_i8(merged.wheel, extra.wheel);
            merged.buttons = extra.buttons;    /* last-write-wins */
            drained++;
        }

        (void)dd_ble_hid_send(&merged);
    }
}
