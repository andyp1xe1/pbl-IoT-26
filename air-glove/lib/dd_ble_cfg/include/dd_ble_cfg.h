#ifndef DD_BLE_CFG_H
#define DD_BLE_CFG_H

/* dd_ble_cfg — custom BLE GATT "config & telemetry" service (Phase II).
 *
 * Sits alongside the HID mouse profile on the SAME NimBLE server created by
 * dd_ble_hid. Lets a companion app (Web Bluetooth) read/write runtime tuning,
 * stream live sensor telemetry, trigger calibration commands, and observe
 * command status. The HID path is untouched: the OS still sees a plain mouse.
 *
 * GATT contract (see docs/plans/10-companion-app.md):
 *   Service     41470001-7a13-4b1e-9c2f-1d0e5f6a7b8c
 *     Config    41470002  R/W     10-byte packed config
 *     Telemetry 41470003  R/Notify 24-byte packed sensor frame
 *     Command   41470004  W       1-byte opcode
 *     Status    41470005  R/Notify 4-byte command status
 *
 * Public header exposes only logical structs — no NimBLE types leak (ADR-005).
 * MUST be initialised AFTER dd_ble_hid_init_server() so the NimBLE server exists.
 */

#include "ag_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Per-pad click action enum. Stored as u8 in click_action[] / click_action_alt[]. */
typedef enum {
    AG_CLICK_NONE        = 0,
    AG_CLICK_LEFT        = 1,
    AG_CLICK_RIGHT       = 2,
    AG_CLICK_MIDDLE      = 3,
    AG_CLICK_SCROLL_UP   = 4,   /* PRESS → one wheel notch up   */
    AG_CLICK_SCROLL_DOWN = 5,   /* PRESS → one wheel notch down */
    AG_CLICK_CLUTCH      = 6,   /* hold to freeze cursor        */
    AG_CLICK_SCROLL_MODE = 7,   /* hold to reroute tilt → wheel */
    AG_CLICK_MAX         = AG_CLICK_SCROLL_MODE,
} ag_click_action_t;

/* Sentinel: modifier_pad == AG_NO_MODIFIER → no chord modifier set. */
#define AG_NO_MODIFIER  ((uint8_t)0xFF)

/* Motion-mix axes (AG_MIX_*) live in ag_types.h so srv_motion can consume
 * the enum without pulling dd_ble_cfg into its include path. */

/* Runtime tuning, in logical units (no wire encoding leaks to callers).
 * Layout mirrors Plan 11.4 / wire format v3 (65 bytes).
 *
 * Motion mapping is: dx = Σ mix_x[i]·signal[i],  dy = Σ mix_y[i]·signal[i],
 * then × sens, then radial deadzone + gain curve + clamp. Each mix weight
 * is a signed ×1000 multiplier ("milli"); range ±2000 = ±2.0. */
typedef struct {
    uint16_t sens_x_milli;             /* X sensitivity ×1000 (1000 = 1.00×) */
    uint16_t sens_y_milli;             /* Y sensitivity ×1000                */
    uint16_t deadzone_mrad;            /* radial deadzone (m-rad)            */
    uint16_t madgwick_beta_milli;      /* Madgwick β ×1000                   */
    uint16_t debounce_ms;              /* per-pad debounce, ms               */
    uint16_t touch_threshold[4];       /* per-pad raw-count threshold        */
    uint8_t  click_action[4];          /* per-pad action (ag_click_action_t) */
    uint8_t  modifier_pad;             /* pad index 0..3, or AG_NO_MODIFIER  */
    uint8_t  click_action_alt[3];      /* alt action for non-modifier pads,
                                          indexed by the non-modifier slot   */
    uint8_t  madgwick_enabled;         /* 1 = run fusion task, expose fused
                                          rates as mix inputs                */
    int16_t  mix_x_milli[AG_MIX_COUNT]; /* per-axis weight into cursor dx    */
    int16_t  mix_y_milli[AG_MIX_COUNT]; /* per-axis weight into cursor dy    */
    uint16_t wrist_roll_comp_milli;    /* wrist-roll compensation strength
                                          ×1000 (0 = off, 1000 = full undo).
                                          Pre-rotates the body-frame angular
                                          increment by −φ·strength about
                                          glove Y so cursor mapping stays
                                          invariant to wrist twist.         */
} dd_ble_cfg_t;

/* One live telemetry frame pushed to the host. */
typedef struct {
    int16_t  accel_mg[3];     /* milli-g                              */
    int16_t  gyro_mdps[3];    /* milli-deg/s                          */
    uint16_t touch[4];        /* raw pad readings: pinky,index,ring,middle */
    uint8_t  battery_pct;     /* 0..100                               */
    uint8_t  flags;           /* see DD_BLE_CFG_TFLAG_*                */
} dd_ble_cfg_telemetry_t;

/* Telemetry flag bits. */
#define DD_BLE_CFG_TFLAG_HID_CONNECTED  0x01
#define DD_BLE_CFG_TFLAG_CALIBRATING    0x02
#define DD_BLE_CFG_TFLAG_SLEEPING       0x04   /* device is in low-power soft-sleep */

/* Command opcodes (host writes these to the Command characteristic). */
#define DD_BLE_CFG_CMD_NONE             0x00
#define DD_BLE_CFG_CMD_CALIBRATE_IMU    0x01
#define DD_BLE_CFG_CMD_RECAL_TOUCH      0x02
#define DD_BLE_CFG_CMD_SAVE             0x03
#define DD_BLE_CFG_CMD_FACTORY_RESET    0x04
#define DD_BLE_CFG_CMD_SLEEP            0x05   /* enter soft-sleep (BLE link stays up) */
#define DD_BLE_CFG_CMD_WAKE             0x06   /* exit soft-sleep, resume mouse reports */
/* Telemetry rate hints. Companion sends one of these as the active tab
 * changes, so we publish only what's actually being watched. Idle for
 * the Home tab (only the SLEEPING flag matters), Normal for Tune (touch
 * bars), Fast for Calibrate (live IMU readouts). */
#define DD_BLE_CFG_CMD_TELE_IDLE        0x07   /* ~4 Hz  (250 ms) */
#define DD_BLE_CFG_CMD_TELE_NORMAL      0x08   /* ~15 Hz (66 ms)  */
#define DD_BLE_CFG_CMD_TELE_FAST        0x09   /* ~20 Hz (50 ms)  */

/* Command status states (mirrored to the host on the Status characteristic). */
#define DD_BLE_CFG_ST_IDLE      0
#define DD_BLE_CFG_ST_RUNNING   1
#define DD_BLE_CFG_ST_SUCCESS   2
#define DD_BLE_CFG_ST_FAIL      3

/* Register the service on the existing NimBLE server and seed config.
 * `defaults` is used only if no config has been persisted to NVS; pass NULL
 * to use built-in defaults. Returns AG_OK, or AG_ERR_* on failure.
 * Call once, from app_controller, AFTER dd_ble_hid_init_server(). */
ag_result_t dd_ble_cfg_init(const dd_ble_cfg_t *defaults);

/* Copy the current (host-writable) config. Thread-safe. */
void dd_ble_cfg_get_config(dd_ble_cfg_t *out);

/* Monotonic counter incremented whenever the config changes (host write,
 * load, or factory reset). Consumers compare against their last-applied
 * value to know when to re-apply. Lock-free 32-bit read. */
uint32_t dd_ble_cfg_config_version(void);

/* Encode + notify one telemetry frame. Safe to call from a single producer
 * task (e.g. t_cfg). No-op if the host has not subscribed. */
void dd_ble_cfg_publish_telemetry(const dd_ble_cfg_telemetry_t *t);

/* Pop a pending host command opcode, or DD_BLE_CFG_CMD_NONE if none.
 * Clears the pending slot. Lock-free. */
uint8_t dd_ble_cfg_take_command(void);

/* Update + notify the command status (opcode being reported, state, 0..100). */
void dd_ble_cfg_set_status(uint8_t opcode, uint8_t state, uint8_t progress);

/* Persist the current config to NVS. Returns AG_OK on success. */
ag_result_t dd_ble_cfg_save(void);

/* Restore built-in defaults, clear NVS, bump the config version. */
void dd_ble_cfg_factory_reset(void);

#ifdef __cplusplus
}
#endif
#endif /* DD_BLE_CFG_H */
