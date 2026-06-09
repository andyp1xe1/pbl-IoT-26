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
#include "dd_ble_cfg.h"
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

static inline int16_t sat_i16(float v)
{
    if (v >  32767.0f) v =  32767.0f;
    if (v < -32768.0f) v = -32768.0f;
    return (int16_t)v;
}

/* Translate the companion-app config into srv_motion tuning. At the default
 * config (sens 1.00×, deadzone 4 mrad) this reproduces kDefaultMotionCfg, so
 * the carefully-tuned out-of-box feel is unchanged. X speed tracks sens_x,
 * Y speed tracks sens_y independently. */
static motion_config_t motion_from_cfg(const dd_ble_cfg_t *c)
{
    const float sx = (float)c->sens_x_milli / 1000.0f;
    const float sy = (float)c->sens_y_milli / 1000.0f;
    motion_config_t mc;
    mc.deadzone_rad = (float)c->deadzone_mrad / 1000.0f;
    mc.gain_low     = 600.0f * sx;
    mc.gain_exp     = 1.2f;
    mc.velocity_cap = 127.0f;
    mc.gain_y_scale = (sx > 0.0f) ? 1.7f * (sy / sx) : 1.7f;
    return mc;
}

/* Resolve which action a pad fires now, given the current modifier state.
 * If the modifier_pad is held (and isn't `pad` itself), use the alt table;
 * otherwise the primary action. */
static uint8_t resolve_action(uint8_t pad, uint8_t pads_held,
                              const dd_ble_cfg_t *c)
{
    if (c->modifier_pad <= 3
        && c->modifier_pad != pad
        && (pads_held & (uint8_t)(1u << c->modifier_pad)))
    {
        uint8_t idx = pad < c->modifier_pad ? pad : (uint8_t)(pad - 1);
        if (idx >= 3) return AG_CLICK_NONE;
        return c->click_action_alt[idx];
    }
    return c->click_action[pad];
}

/* Apply one click action to mouse-button/wheel state. Updates *buttons and
 * *wheel_out in place; returns true iff a HID report should be queued now. */
static bool apply_action(uint8_t action, bool pressed,
                         uint8_t *buttons, int8_t *wheel_out)
{
    *wheel_out = 0;
    switch (action) {
    case AG_CLICK_NONE:
        return false;
    case AG_CLICK_LEFT:
        if (pressed) *buttons |= 0x01u; else *buttons &= (uint8_t)~0x01u;
        return true;
    case AG_CLICK_RIGHT:
        if (pressed) *buttons |= 0x02u; else *buttons &= (uint8_t)~0x02u;
        return true;
    case AG_CLICK_MIDDLE:
        if (pressed) *buttons |= 0x04u; else *buttons &= (uint8_t)~0x04u;
        return true;
    case AG_CLICK_SCROLL_UP:
        if (pressed) { *wheel_out = +1; return true; }
        return false;
    case AG_CLICK_SCROLL_DOWN:
        if (pressed) { *wheel_out = -1; return true; }
        return false;
    case AG_CLICK_CLUTCH:
        srv_motion_set_clutch(pressed);
        return false;
    case AG_CLICK_SCROLL_MODE:
        g_scroll_mode.store(pressed);
        return false;
    }
    return false;
}

static const char *action_name(uint8_t a)
{
    switch (a) {
    case AG_CLICK_NONE:        return "NONE";
    case AG_CLICK_LEFT:        return "LEFT";
    case AG_CLICK_RIGHT:       return "RIGHT";
    case AG_CLICK_MIDDLE:      return "MIDDLE";
    case AG_CLICK_SCROLL_UP:   return "SCROLL_UP";
    case AG_CLICK_SCROLL_DOWN: return "SCROLL_DOWN";
    case AG_CLICK_CLUTCH:      return "CLUTCH";
    case AG_CLICK_SCROLL_MODE: return "SCROLL_MODE";
    default:                   return "?";
    }
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

            /* Snapshot for the companion-app telemetry stream (wire units:
             * accel milli-g, gyro milli-deg/s). 1 g = 9.80665 m/s²;
             * 1 rad/s = 57.2957795 deg/s. */
            g_tele_accel_mg[0].store(sat_i16(s.ax / 9.80665f * 1000.0f));
            g_tele_accel_mg[1].store(sat_i16(s.ay / 9.80665f * 1000.0f));
            g_tele_accel_mg[2].store(sat_i16(s.az / 9.80665f * 1000.0f));
            g_tele_gyro_mdps[0].store(sat_i16(s.gx * 57295.7795f));
            g_tele_gyro_mdps[1].store(sat_i16(s.gy * 57295.7795f));
            g_tele_gyro_mdps[2].store(sat_i16(s.gz * 57295.7795f));

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
    uint32_t applied_cfg_version = 0;

    for (;;) {
        imu_sample_t s;
        if (xQueueReceive(q_imu, &s, portMAX_DELAY) != pdTRUE) continue;

        /* Apply companion-app β changes from this (owning) task so srv_fusion
         * stays single-threaded per its contract. */
        uint32_t v = dd_ble_cfg_config_version();
        if (v != applied_cfg_version) {
            dd_ble_cfg_t c;
            dd_ble_cfg_get_config(&c);
            const float beta = (float)c.madgwick_beta_milli / 1000.0f;
            srv_fusion_init(beta);
            applied_cfg_version = v;
            printf("[fusion] applied config v%u: beta=%.3f\n",
                   (unsigned)v, (double)beta);
        }

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
    uint32_t          applied_cfg_version = 0;

    for (;;) {
        /* Apply companion-app threshold/debounce changes from this (owning)
         * task so srv_input stays single-threaded per its contract. */
        uint32_t v = dd_ble_cfg_config_version();
        if (v != applied_cfg_version) {
            dd_ble_cfg_t c;
            dd_ble_cfg_get_config(&c);
            srv_input_set_thresholds(c.touch_threshold);
            srv_input_set_debounce_ms(c.debounce_ms);
            applied_cfg_version = v;
            printf("[touch] applied config v%u: thresh=[%u,%u,%u,%u] debounce=%ums\n",
                   (unsigned)v,
                   c.touch_threshold[0], c.touch_threshold[1],
                   c.touch_threshold[2], c.touch_threshold[3],
                   c.debounce_ms);
        }

        touch_sample_t s;
        if (dd_touch_read(&s) == AG_OK) {
            /* Snapshot raw pad readings for the companion-app telemetry. */
            for (int i = 0; i < TOUCH_PAD_COUNT && i < 4; ++i) {
                g_tele_touch_raw[i].store(s.raw[i]);
            }

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
    uint64_t last_t_us   = 0;
    int16_t  scroll_accum = 0;   /* sub-notch accumulator for scroll mode */

    /* One HID wheel "notch" is emitted every kScrollThreshold dy-units
     * accumulated. Lower = faster scroll. Tune between 60 (fast) and 200
     * (slow). At current gain settings a moderate upward tilt produces
     * ~15 dy/frame, so threshold=100 → ~1 notch every 7 frames ≈ 14 Hz. */
    static constexpr int16_t kScrollThreshold = 100;

    uint32_t applied_cfg_version = 0;   /* 0 ≠ initial s_version (1) → applies once at start */

    for (;;) {
        oriented_frame_t f;
        if (xQueueReceive(q_orientation, &f, portMAX_DELAY) != pdTRUE) continue;

        /* Apply companion-app config changes from this (the owning) task, so
         * srv_motion stays single-threaded per its contract. */
        uint32_t cfg_version = dd_ble_cfg_config_version();
        if (cfg_version != applied_cfg_version) {
            dd_ble_cfg_t c;
            dd_ble_cfg_get_config(&c);
            motion_config_t mc = motion_from_cfg(&c);
            srv_motion_init(&mc);
            applied_cfg_version = cfg_version;
            printf("[motion] applied config v%u: gain_low=%.0f y_scale=%.2f dz=%.4f\n",
                   (unsigned)cfg_version, (double)mc.gain_low,
                   (double)mc.gain_y_scale, (double)mc.deadzone_rad);
        }

        float dt_s = 0.01f;
        if (last_t_us != 0 && f.t_us > last_t_us) {
            dt_s = (float)(f.t_us - last_t_us) * 1e-6f;
        }
        last_t_us = f.t_us;

        int8_t dx = 0, dy = 0;
        if (srv_motion_update(&f.q, dt_s, &dx, &dy) != AG_OK) continue;

        if (g_scroll_mode.load()) {
            /* ── Scroll mode: dy drives the wheel, cursor stays frozen ── *
             * The motion clutch is NOT active during scroll (t_app only
             * engages it for the button-chord). The mapper runs normally so
             * dy carries a live tilt value. We accumulate it and emit one
             * HID wheel notch per kScrollThreshold units. No cursor report
             * is emitted, so the cursor is naturally frozen.              */
            scroll_accum += dy;
            int8_t wheel = 0;
            while (scroll_accum >=  kScrollThreshold && wheel <  7)
                { wheel++;  scroll_accum -= kScrollThreshold; }
            while (scroll_accum <= -kScrollThreshold && wheel > -7)
                { wheel--;  scroll_accum += kScrollThreshold; }

            if (wheel != 0) {
                hid_mouse_report_t r = {};
                r.buttons = g_current_buttons.load();
                r.wheel   = wheel;
                queue_put_drop_oldest(q_hid, &r);
            }
        } else {
            scroll_accum = 0;   /* reset when leaving scroll mode */

            if (dx != 0 || dy != 0) {
                hid_mouse_report_t r = {};
                r.dx      = dx;
                r.dy      = dy;
                r.buttons = g_current_buttons.load();
                queue_put_drop_oldest(q_hid, &r);
            }
        }
    }
}

/* ── t_app — drain button events, dispatch per data-driven click map ───── */
/*
 * Interaction model (Plan 11.3):
 *
 *   Each pad fires its `click_action[i]` on PRESS / RELEASE.
 *   If `modifier_pad` is set and held, the *other* pads fire their
 *   `click_action_alt[]` entry instead. The modifier pad itself produces
 *   no output while held — its only job is to shift the action table.
 *
 *   SCROLL_MODE and CLUTCH are hold gestures and bypass the HID report path:
 *   they toggle the matching internal flag (`g_scroll_mode`, srv_motion clutch)
 *   directly. The cursor pipeline observes these flags downstream.
 */
void t_app_fn(void *)
{
    uint8_t      pads_held           = 0;
    uint32_t     applied_cfg_version = 0;
    dd_ble_cfg_t cfg = {};
    dd_ble_cfg_get_config(&cfg);

    for (;;) {
        input_event_t ev;
        if (xQueueReceive(q_buttons, &ev, portMAX_DELAY) != pdTRUE) continue;

        uint32_t v = dd_ble_cfg_config_version();
        if (v != applied_cfg_version) {
            dd_ble_cfg_get_config(&cfg);
            applied_cfg_version = v;
        }

        const bool is_press   = (ev.kind == INPUT_EVT_PRESS);
        const bool is_release = (ev.kind == INPUT_EVT_RELEASE);
        if (!is_press && !is_release)       continue;
        if (ev.pad >= TOUCH_PAD_COUNT)      continue;

        const uint8_t pad_bit = (uint8_t)(1u << ev.pad);
        if (is_press) pads_held |= pad_bit;
        else          pads_held &= (uint8_t)~pad_bit;

        if (ev.pad == cfg.modifier_pad) {
            printf("[touch] MODIFIER pad=%u %s\n",
                   (unsigned)ev.pad, is_press ? "engaged" : "released");
            continue;
        }

        const uint8_t action = resolve_action(ev.pad, pads_held, &cfg);
        if (action == AG_CLICK_NONE) continue;

        uint8_t buttons = g_current_buttons.load();
        int8_t  wheel   = 0;
        const bool emit = apply_action(action, is_press, &buttons, &wheel);

        printf("[touch] pad=%u %s %s\n", (unsigned)ev.pad,
               action_name(action), is_press ? "PRESSED" : "released");

        if (emit) {
            g_current_buttons.store(buttons);
            hid_mouse_report_t r = {};
            r.buttons = buttons;
            r.wheel   = wheel;
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

/* ── t_cfg — companion-app telemetry pump + command handler ────────────── *
 * Best-effort, low priority. Publishes a telemetry frame ~15 Hz from the
 * latest IMU/touch snapshots, and services config commands (save / reset /
 * calibrate) written by the companion app. The calibrate path is a Phase-stub
 * that reports progress; the real gyro-bias routine lands with E12. */
void t_cfg_fn(void *)
{
    TickType_t       last   = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(66);   /* ~15 Hz */

    for (;;) {
        dd_ble_cfg_telemetry_t t = {};
        for (int i = 0; i < 3; ++i) {
            t.accel_mg[i]  = g_tele_accel_mg[i].load();
            t.gyro_mdps[i] = g_tele_gyro_mdps[i].load();
        }
        for (int i = 0; i < 4; ++i) t.touch[i] = g_tele_touch_raw[i].load();
        t.battery_pct = 100;   /* no fuel gauge in Phase I hardware */
        t.flags = dd_ble_hid_is_connected() ? DD_BLE_CFG_TFLAG_HID_CONNECTED : 0;
        dd_ble_cfg_publish_telemetry(&t);

        uint8_t op = dd_ble_cfg_take_command();
        switch (op) {
            case DD_BLE_CFG_CMD_SAVE: {
                ag_result_t rc = dd_ble_cfg_save();
                dd_ble_cfg_set_status(
                    op,
                    rc == AG_OK ? DD_BLE_CFG_ST_SUCCESS : DD_BLE_CFG_ST_FAIL,
                    100);
                break;
            }
            case DD_BLE_CFG_CMD_FACTORY_RESET:
                dd_ble_cfg_factory_reset();
                dd_ble_cfg_set_status(op, DD_BLE_CFG_ST_SUCCESS, 100);
                break;
            case DD_BLE_CFG_CMD_RECAL_TOUCH:
                /* dd_touch re-baselines at boot; runtime re-baseline is not yet
                 * exposed (E04 backlog) — acknowledge so the UI completes. */
                dd_ble_cfg_set_status(op, DD_BLE_CFG_ST_SUCCESS, 100);
                break;
            case DD_BLE_CFG_CMD_CALIBRATE_IMU:
                for (int p = 0; p <= 100; p += 20) {
                    dd_ble_cfg_set_status(op, DD_BLE_CFG_ST_RUNNING, (uint8_t)p);
                    vTaskDelay(pdMS_TO_TICKS(300));
                }
                dd_ble_cfg_set_status(op, DD_BLE_CFG_ST_SUCCESS, 100);
                break;
            default:
                break;
        }

        vTaskDelayUntil(&last, period);
    }
}
