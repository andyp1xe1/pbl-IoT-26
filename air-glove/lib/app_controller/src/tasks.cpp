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
    uint32_t          count  = 0;

    for (;;) {
        touch_sample_t s;
        if (dd_touch_read(&s) == AG_OK) {
            /* Log raw touch values every 2 s so you can see live readings
             * vs thresholds — useful for diagnosing wire contact issues. */
            if (++count % 200 == 0) {
                printf("[touch] raw  thumb:%4u  index:%4u  middle:%4u  "
                       "mask=0x%02X\n",
                       s.raw[0], s.raw[1], s.raw[2], s.touched_mask);
            }

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
/*
 * Interaction model:
 *
 *   index alone   → left click  (HID button bit 0)
 *   middle alone  → right click (HID button bit 1)
 *   index+middle  → CLUTCH: cursor freezes; reposition hand; release to resume
 *
 * Clutch lets you physically return your wrist to a neutral angle without
 * the cursor following. The chord entry releases any held HID buttons so the
 * host never sees a stuck click. Individual button events are suppressed on
 * clutch exit so the release gesture doesn't register as a spurious click.
 */
void t_app_fn(void *)
{
    bool index_held  = false;
    bool middle_held = false;
    bool clutch_on   = false;

    for (;;) {
        input_event_t ev;
        if (xQueueReceive(q_buttons, &ev, portMAX_DELAY) != pdTRUE) continue;

        if (ev.pad != TOUCH_PAD_INDEX && ev.pad != TOUCH_PAD_MIDDLE) continue;

        const bool is_press   = (ev.kind == INPUT_EVT_PRESS);
        const bool is_release = (ev.kind == INPUT_EVT_RELEASE);
        if (!is_press && !is_release) continue;

        /* Update per-finger held state before evaluating the chord. */
        if (ev.pad == TOUCH_PAD_INDEX)  index_held  = is_press;
        if (ev.pad == TOUCH_PAD_MIDDLE) middle_held = is_press;

        const bool both_held = index_held && middle_held;

        /* ── Clutch entry: both fingers now held ──────────────────────── */
        if (both_held && !clutch_on) {
            clutch_on = true;
            srv_motion_set_clutch(true);
            /* Release any held HID button so host doesn't see a stuck click. */
            g_current_buttons.store(0);
            hid_mouse_report_t r = {};
            queue_put_drop_oldest(q_hid, &r);
            printf("[touch] CLUTCH on  — reposition hand freely\n");
            continue;
        }

        /* ── Clutch exit: one finger lifted while clutch was active ───── */
        if (!both_held && clutch_on) {
            clutch_on = false;
            srv_motion_set_clutch(false);
            /* Suppress the releasing finger's event so it doesn't register
             * as a spurious click after the reposition. */
            g_current_buttons.store(0);
            printf("[touch] CLUTCH off — cursor active\n");
            continue;
        }

        /* ── Inside clutch: ignore all button changes ─────────────────── */
        if (clutch_on) continue;

        /* ── Normal single-finger button handling ─────────────────────── */
        const uint8_t bit  = (ev.pad == TOUCH_PAD_INDEX) ? 0x01u : 0x02u;
        const char *name   = (bit == 0x01) ? "LEFT (index)" : "RIGHT (middle)";

        uint8_t prev = g_current_buttons.load();
        uint8_t next = prev;
        if (is_press)   next |=  bit;
        if (is_release) next &= ~bit;

        if (next != prev) {
            g_current_buttons.store(next);
            printf("[touch] %s %s\n", name, is_press ? "PRESSED" : "released");
            hid_mouse_report_t r = {};
            r.buttons = next;
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
        if (xQueueReceive(q_hid, &merged, pdMS_TO_TICKS(15)) != pdTRUE) {
            /* No report in the last 15 ms (~67 Hz pacing). This aligns with
             * the typical 15 ms Windows BLE HID connection interval so each
             * notify() maps to at most one connection event, preventing the
             * NimBLE TX queue from filling up and destabilising the link. */
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
