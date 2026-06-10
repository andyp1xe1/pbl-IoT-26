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

/* Translate the companion-app config into srv_motion tuning. The mix matrix
 * + sens + deadzone are passed straight through; gain curve / velocity cap
 * / EMA stay internal to srv_motion. */
static motion_config_t motion_from_cfg(const dd_ble_cfg_t *c)
{
    motion_config_t mc = {};
    for (int i = 0; i < AG_MIX_COUNT; ++i) {
        mc.mix_x_milli[i] = c->mix_x_milli[i];
        mc.mix_y_milli[i] = c->mix_y_milli[i];
    }
    mc.sens_x_milli = c->sens_x_milli;
    mc.sens_y_milli = c->sens_y_milli;
    mc.deadzone_rad = (float)c->deadzone_mrad / 1000.0f;
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

    for (;;) {
        /* Soft-sleep: MPU is powered down (dd_mpu6050_set_sleep) so a read
         * would return stale data anyway. Idle at 2 Hz so the wake transition
         * still picks up promptly without burning CPU on a tight delay loop. */
        if (g_sleeping.load()) {
            vTaskDelay(pdMS_TO_TICKS(500));
            last = xTaskGetTickCount();
            continue;
        }

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

            /* Gyro-bias calibration accumulator. Fixed-point milli-rad/s × 1024
             * preserves enough resolution to bias-correct a ~10 mrad/s bias
             * accurately after averaging ~300 samples. */
            if (g_gyro_cal_running.load(std::memory_order_acquire)) {
                g_gyro_cal_sum[0].fetch_add((int64_t)(s.gx * 1024000.0f));
                g_gyro_cal_sum[1].fetch_add((int64_t)(s.gy * 1024000.0f));
                g_gyro_cal_sum[2].fetch_add((int64_t)(s.gz * 1024000.0f));
                g_gyro_cal_count.fetch_add(1u);
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

        /* Soft-sleep: keep the touch sensor running so g_tele_touch_raw still
         * updates for the companion's Tune screen, but throttle to 2 Hz and
         * suppress event emission so a pad accidentally grazed while the glove
         * is set down can't queue a phantom click. */
        const bool sleeping = g_sleeping.load();

        touch_sample_t s;
        if (dd_touch_read(&s) == AG_OK) {
            /* Snapshot raw pad readings for the companion-app telemetry. */
            for (int i = 0; i < TOUCH_PAD_COUNT && i < 4; ++i) {
                g_tele_touch_raw[i].store(s.raw[i]);
            }

            /* Touch-baseline calibration accumulator. Sum raw cap-pad reads
             * for all capacitive pads while the running flag is set; t_cfg
             * averages and applies when sampling finishes. Button pads
             * contribute their meaningless 0/4095 read — t_cfg ignores those
             * indices via dd_touch_set_baselines (which is_button-gated). */
            if (g_touch_cal_running.load(std::memory_order_acquire)) {
                for (int i = 0; i < TOUCH_PAD_COUNT && i < 4; ++i) {
                    g_touch_cal_sum[i].fetch_add((uint32_t)s.raw[i]);
                }
                g_touch_cal_count.fetch_add(1u);
            }

            /* Edge-triggered raw log: print as soon as ANY pad changes
             * meaningfully (button: any flip, cap: ≥20 count delta), so
             * bench-testing bare wires gives instant feedback instead of
             * waiting for the 2-second heartbeat window. The heartbeat
             * stays so the user can see the values when idle too. */
            static uint16_t last_raw[TOUCH_PAD_COUNT] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};
            static const char *kPadName[TOUCH_PAD_COUNT] =
                {"thumb", "index", "middle", "ring"};
            static const uint8_t kPadGpio[TOUCH_PAD_COUNT] = {4, 14, 15, 13};
            for (int i = 0; i < TOUCH_PAD_COUNT; ++i) {
                int delta = (int)s.raw[i] - (int)last_raw[i];
                int adelta = delta < 0 ? -delta : delta;
                /* Buttons swing 4095↔0 so any change is significant; cap pad
                 * drifts with EMA so require a real step. */
                int threshold = (i == TOUCH_PAD_THUMB) ? 20 : 1000;
                if (adelta >= threshold) {
                    printf("[touch] %s (GPIO%u): %u -> %u\n",
                           kPadName[i], kPadGpio[i],
                           last_raw[i], s.raw[i]);
                    last_raw[i] = s.raw[i];
                }
            }

            /* 2 s heartbeat — keeps the live values visible even when
             * nothing is changing, so you can sanity-check the baseline. */
            if (++count % 200 == 0) {
                printf("[touch] raw  thumb:%4u  index:%4u  middle:%4u  "
                       "ring:%4u  mask=0x%02X\n",
                       s.raw[0], s.raw[1], s.raw[2], s.raw[3],
                       s.touched_mask);
            }

            if (!sleeping) {
                input_event_t evts[TOUCH_PAD_COUNT];
                size_t n = 0;
                if (srv_input_process(&s, evts, TOUCH_PAD_COUNT, &n) == AG_OK) {
                    for (size_t i = 0; i < n; ++i) {
                        queue_put_drop_oldest(q_buttons, &evts[i]);
                    }
                }
            }
        }
        if (sleeping) {
            vTaskDelay(pdMS_TO_TICKS(500));
            last = xTaskGetTickCount();
        } else {
            vTaskDelayUntil(&last, period);
        }
    }
}

/* ── t_motion — q_orientation → dx/dy → q_hid ─────────────────────────── */
void t_motion_fn(void *)
{
    int16_t  scroll_accum = 0;   /* sub-notch accumulator for scroll mode */

    /* One HID wheel "notch" is emitted every kScrollThreshold dy-units
     * accumulated. Lower = faster scroll. Tune between 60 (fast) and 200
     * (slow). */
    static constexpr int16_t kScrollThreshold = 100;
    static constexpr float kDegToRad = 0.017453292519943f;

    uint32_t applied_cfg_version = 0;   /* 0 ≠ initial s_version (1) → applies once at start */
    bool     madgwick_on        = true;
    bool     has_prev_q         = false;
    quat_t   prev_q             = { 1.0f, 0.0f, 0.0f, 0.0f };
    float    wrist_comp_strength = 1.0f;   /* 0..1, set from cfg on apply */

    for (;;) {
        oriented_frame_t f;
        if (xQueueReceive(q_orientation, &f, portMAX_DELAY) != pdTRUE) continue;

        /* Soft-sleep: drop any frame already in flight when sleep was entered.
         * t_imu_sample stops producing once g_sleeping flips so the queue will
         * drain on its own; this just guards the race window. */
        if (g_sleeping.load()) continue;

        /* Apply companion-app config changes from this (the owning) task, so
         * srv_motion stays single-threaded per its contract. */
        uint32_t cfg_version = dd_ble_cfg_config_version();
        if (cfg_version != applied_cfg_version) {
            dd_ble_cfg_t c;
            dd_ble_cfg_get_config(&c);
            motion_config_t mc = motion_from_cfg(&c);
            srv_motion_init(&mc);
            madgwick_on         = (c.madgwick_enabled != 0);
            wrist_comp_strength = (float)c.wrist_roll_comp_milli * 0.001f;
            applied_cfg_version = cfg_version;
            printf("[motion] applied config v%u: madgwick=%u sens=[%u,%u] dz=%.4f comp=%.3f\n",
                   (unsigned)cfg_version, (unsigned)madgwick_on,
                   c.sens_x_milli, c.sens_y_milli, (double)mc.deadzone_rad,
                   (double)wrist_comp_strength);
        }

        /* Build the 6-axis cursor signal vector. Wrist-twist lanes (GY/AY,
         * and the body-Y rotation rate) are intentionally absent — they're
         * non-gesture and decoupled from cursor motion entirely. */
        const float gx_raw = (float)g_tele_gyro_mdps[0].load() * 1e-3f * kDegToRad;
        const float gz_raw = (float)g_tele_gyro_mdps[2].load() * 1e-3f * kDegToRad;
        const float ax_raw = (float)g_tele_accel_mg[0].load()  * 1e-3f * 9.80665f;
        const float az_raw = (float)g_tele_accel_mg[2].load()  * 1e-3f * 9.80665f;

        /* Wrist-roll compensation. φ = wrist-twist angle about glove Y
         * (extracted from where world-up lands in the body XZ plane).
         * The current body frame is rotated by +φ about Y relative to
         * neutral, so a vector expressed in current-body components is
         * mapped to neutral-body components by applying R(+φ) about Y.
         * strength scales φ: 0 → no compensation; 1 → full undo.
         *
         * Runs every frame, independent of the Madgwick toggle: t_fusion
         * always produces a valid q regardless of `madgwick_enabled`, and
         * the raw GX/GZ/AX/AZ lanes need compensation just as much as the
         * fused lanes do. Madgwick toggle only gates the fused ROLL/YAW
         * signals (pure-raw-IMU mode for experimentation). */
        const float qw = f.q.q0, qx = f.q.q1, qy = f.q.q2, qz = f.q.q3;
        const float phi = atan2f(2.0f * (qw*qy - qx*qz),
                                 1.0f - 2.0f * (qx*qx + qy*qy));
        const float theta = phi * wrist_comp_strength;
        const float cs    = cosf(theta);
        const float sn    = sinf(theta);

        float signals[AG_MIX_COUNT];
        signals[AG_MIX_GX] =  cs * gx_raw + sn * gz_raw;
        signals[AG_MIX_GZ] = -sn * gx_raw + cs * gz_raw;
        signals[AG_MIX_AX] =  cs * ax_raw + sn * az_raw;
        signals[AG_MIX_AZ] = -sn * ax_raw + cs * az_raw;

        if (madgwick_on && has_prev_q) {
            /* Body-frame quaternion delta: q_delta = prev_q^-1 ⊗ f.q.
             * 2·vector(q_delta) ≈ rotation-vector in the previous body frame. */
            const float p0 =  prev_q.q0, p1 = -prev_q.q1, p2 = -prev_q.q2, p3 = -prev_q.q3;
            const float c0 = f.q.q0,    c1 = f.q.q1,    c2 = f.q.q2,    c3 = f.q.q3;
            const float vx_b = 2.0f * (p0*c1 + p1*c0 + p2*c3 - p3*c2);
            const float vz_b = 2.0f * (p0*c3 + p1*c2 - p2*c1 + p3*c0);
            /* (vy_b — rotation about glove Y = wrist twist — is discarded.) */
            signals[AG_MIX_ROLL] =  cs * vx_b + sn * vz_b;
            signals[AG_MIX_YAW]  = -sn * vx_b + cs * vz_b;
        } else {
            signals[AG_MIX_ROLL] = signals[AG_MIX_YAW] = 0.0f;
        }
        prev_q     = f.q;
        has_prev_q = true;

        int8_t dx = 0, dy = 0;
        if (srv_motion_update(signals, &dx, &dy) != AG_OK) continue;

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

        /* Soft-sleep: t_touch stops queueing events, but a few may sit in
         * q_buttons at the moment of sleep entry — drop them so the OS
         * doesn't get a phantom release after the device "went to sleep". */
        if (g_sleeping.load()) continue;

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
        if (xQueueReceive(q_hid, &merged, pdMS_TO_TICKS(10)) != pdTRUE) {
            /* No report in the last 10 ms (~100 Hz pacing). On Linux BlueZ
             * the negotiated HID conn interval is typically 7.5–11.25 ms, so
             * we want to be ready to fill every event slot. Windows used to
             * negotiate ~15 ms and earlier code paced at 15 ms to match,
             * but NimBLE silently drops notifies when the TX queue is full
             * and `queue_put_drop_oldest` upstream prevents back-pressure
             * stalls, so over-pumping at 10 ms is safe on either host. */
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
 * Best-effort, low priority. Publishes a telemetry frame ~5 Hz from the
 * latest IMU/touch snapshots, and services config commands (save / reset /
 * calibrate) written by the companion app. The calibrate path is a Phase-stub
 * that reports progress; the real gyro-bias routine lands with E12.
 *
 * Rate rationale: telemetry is shown to a human in the companion UI; 5 Hz is
 * smooth enough for live sensor read-outs and leaves BLE connection-event
 * slots free for the HID input-report path. dd_ble_cfg additionally
 * short-circuits the notify when no host has the telemetry CCC enabled, so
 * with the companion closed this task is effectively a no-op on the air.   */
void t_cfg_fn(void *)
{
    TickType_t last = xTaskGetTickCount();

    for (;;) {
        /* Read the rate at the top of each iteration so a rate-change
         * command takes effect immediately on the next tick instead of
         * the iteration after. Clamp at 15 ms (≈66 Hz) — Linux BlueZ
         * negotiates HID conn intervals around 7.5-11.25 ms so this lets
         * a telemetry notify ride every couple of conn events without
         * contending with the HID input report path. */
        TickType_t period = pdMS_TO_TICKS(g_telemetry_period_ms.load());
        if (period < pdMS_TO_TICKS(15)) period = pdMS_TO_TICKS(15);

        dd_ble_cfg_telemetry_t t = {};
        for (int i = 0; i < 3; ++i) {
            t.accel_mg[i]  = g_tele_accel_mg[i].load();
            t.gyro_mdps[i] = g_tele_gyro_mdps[i].load();
        }
        for (int i = 0; i < 4; ++i) t.touch[i] = g_tele_touch_raw[i].load();
        t.battery_pct = 100;   /* no fuel gauge in Phase I hardware */
        uint8_t flags = 0;
        if (dd_ble_hid_is_connected()) flags |= DD_BLE_CFG_TFLAG_HID_CONNECTED;
        if (g_sleeping.load())         flags |= DD_BLE_CFG_TFLAG_SLEEPING;
        t.flags = flags;
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
            case DD_BLE_CFG_CMD_RECAL_TOUCH: {
                /* Hold-no-touch baseline calibration. Same pattern as the gyro
                 * cal: reset accumulator, set running flag, t_touch contributes
                 * raw pad reads at 100 Hz, average to per-pad baseline. */
                constexpr uint32_t kSamples = 300;
                if (g_sleeping.load()) {
                    dd_ble_cfg_set_status(op, DD_BLE_CFG_ST_FAIL, 0);
                    printf("[cfg] touch cal refused: device sleeping\n");
                    break;
                }
                for (int i = 0; i < 4; ++i) g_touch_cal_sum[i].store(0);
                g_touch_cal_count.store(0);
                g_touch_cal_running.store(true, std::memory_order_release);
                dd_ble_cfg_set_status(op, DD_BLE_CFG_ST_RUNNING, 0);

                uint32_t got = 0;
                while ((got = g_touch_cal_count.load()) < kSamples) {
                    uint8_t pct = (uint8_t)((got * 100u) / kSamples);
                    dd_ble_cfg_set_status(op, DD_BLE_CFG_ST_RUNNING, pct);
                    vTaskDelay(pdMS_TO_TICKS(100));
                }
                g_touch_cal_running.store(false, std::memory_order_release);

                const uint32_t n = g_touch_cal_count.load();
                uint16_t bl[TOUCH_PAD_COUNT] = {0, 0, 0, 0};
                for (int i = 0; i < TOUCH_PAD_COUNT && i < 4; ++i) {
                    bl[i] = (uint16_t)(g_touch_cal_sum[i].load() / n);
                }
                dd_touch_set_baselines(bl);
                ag_result_t save_rc = dd_touch_save_baselines();
                printf("[cfg] touch cal: n=%u thumb_baseline=%u save=%d\n",
                       (unsigned)n, bl[TOUCH_PAD_THUMB], (int)save_rc);
                dd_ble_cfg_set_status(
                    op,
                    save_rc == AG_OK ? DD_BLE_CFG_ST_SUCCESS : DD_BLE_CFG_ST_FAIL,
                    100);
                break;
            }
            case DD_BLE_CFG_CMD_CALIBRATE_IMU: {
                /* Hold-still gyro bias calibration. Reset the accumulator,
                 * flip the running flag so t_imu_sample feeds samples in, and
                 * poll progress until kSamples reads have landed (~3 s at the
                 * 100 Hz sample rate). On finish, divide accumulator/count to
                 * get the per-axis bias in rad/s, apply, persist to NVS. */
                constexpr uint32_t kSamples = 300;
                if (g_sleeping.load()) {
                    dd_ble_cfg_set_status(op, DD_BLE_CFG_ST_FAIL, 0);
                    printf("[cfg] gyro cal refused: device sleeping\n");
                    break;
                }
                g_gyro_cal_sum[0].store(0);
                g_gyro_cal_sum[1].store(0);
                g_gyro_cal_sum[2].store(0);
                g_gyro_cal_count.store(0);
                g_gyro_cal_running.store(true, std::memory_order_release);
                dd_ble_cfg_set_status(op, DD_BLE_CFG_ST_RUNNING, 0);

                uint32_t got = 0;
                while ((got = g_gyro_cal_count.load()) < kSamples) {
                    uint8_t pct = (uint8_t)((got * 100u) / kSamples);
                    dd_ble_cfg_set_status(op, DD_BLE_CFG_ST_RUNNING, pct);
                    vTaskDelay(pdMS_TO_TICKS(100));
                }
                g_gyro_cal_running.store(false, std::memory_order_release);

                const int64_t sx = g_gyro_cal_sum[0].load();
                const int64_t sy = g_gyro_cal_sum[1].load();
                const int64_t sz = g_gyro_cal_sum[2].load();
                const uint32_t n = g_gyro_cal_count.load();
                /* Accumulator captured post-subtraction residuals (dd_mpu6050_read
                 * already removes the active bias). Compose with the existing
                 * bias so re-calibrating after a reboot converges to the true
                 * bias instead of overwriting a good value with ~0. */
                const float rbx = (float)sx / ((float)n * 1024000.0f);
                const float rby = (float)sy / ((float)n * 1024000.0f);
                const float rbz = (float)sz / ((float)n * 1024000.0f);
                float pbx = 0.0f, pby = 0.0f, pbz = 0.0f;
                dd_mpu6050_get_gyro_bias(&pbx, &pby, &pbz);
                const float bx = pbx + rbx;
                const float by = pby + rby;
                const float bz = pbz + rbz;
                dd_mpu6050_set_gyro_bias(bx, by, bz);
                ag_result_t save_rc = dd_mpu6050_save_gyro_bias();
                printf("[cfg] gyro cal: n=%u bias=[%.4f %.4f %.4f] rad/s save=%d\n",
                       (unsigned)n, (double)bx, (double)by, (double)bz, (int)save_rc);
                dd_ble_cfg_set_status(
                    op,
                    save_rc == AG_OK ? DD_BLE_CFG_ST_SUCCESS : DD_BLE_CFG_ST_FAIL,
                    100);
                break;
            }
            case DD_BLE_CFG_CMD_SLEEP: {
                /* Order matters: flip the flag first so the worker tasks stop
                 * touching the I2C bus (their next iteration sees g_sleeping
                 * and short-circuits). Only then power the MPU down — otherwise
                 * t_imu_sample could race a read against the SLEEP write. */
                g_sleeping.store(true);
                vTaskDelay(pdMS_TO_TICKS(15));   /* let t_imu_sample finish its iter */
                ag_result_t rc = dd_mpu6050_set_sleep(true);
                if (rc != AG_OK) printf("[cfg] mpu sleep rc=%d\n", rc);
                dd_ble_cfg_set_status(op, DD_BLE_CFG_ST_SUCCESS, 100);
                printf("[cfg] entered soft-sleep\n");
                break;
            }
            case DD_BLE_CFG_CMD_WAKE: {
                ag_result_t rc = dd_mpu6050_set_sleep(false);
                if (rc != AG_OK) printf("[cfg] mpu wake rc=%d\n", rc);
                vTaskDelay(pdMS_TO_TICKS(35));   /* MPU specs ~30 ms to settle */
                g_sleeping.store(false);
                dd_ble_cfg_set_status(op, DD_BLE_CFG_ST_SUCCESS, 100);
                printf("[cfg] woke from soft-sleep\n");
                break;
            }
            case DD_BLE_CFG_CMD_TELE_IDLE:
                g_telemetry_period_ms.store(250);
                dd_ble_cfg_set_status(op, DD_BLE_CFG_ST_SUCCESS, 100);
                printf("[cfg] telemetry rate -> 4 Hz\n");
                break;
            case DD_BLE_CFG_CMD_TELE_NORMAL:
                g_telemetry_period_ms.store(66);
                dd_ble_cfg_set_status(op, DD_BLE_CFG_ST_SUCCESS, 100);
                printf("[cfg] telemetry rate -> 15 Hz\n");
                break;
            case DD_BLE_CFG_CMD_TELE_FAST:
                g_telemetry_period_ms.store(50);
                dd_ble_cfg_set_status(op, DD_BLE_CFG_ST_SUCCESS, 100);
                printf("[cfg] telemetry rate -> 20 Hz\n");
                break;
            default:
                break;
        }

        vTaskDelayUntil(&last, period);
    }
}
