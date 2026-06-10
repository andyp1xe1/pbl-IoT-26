/* dd_ble_cfg — custom GATT config & telemetry service (esp32dev).
 *
 * Adds a second service to the NimBLE server created by dd_ble_hid. ADR-005
 * permits NimBLE includes here because this is a dd_* lib; the public header
 * exposes only logical structs. NimBLE-Arduino 1.x API (h2zero ^1.4.0).
 *
 * Threading: host writes land on the NimBLE host task; firmware tasks read
 * config snapshots and push telemetry. Shared config is guarded by a portMUX
 * spinlock. The pending-command byte and 32-bit version are single-word
 * volatile accesses (atomic on ESP32).
 */

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <NimBLEServer.h>
#include <Preferences.h>
#include <stdio.h>
#include <string.h>

#include "dd_ble_cfg.h"

namespace {

constexpr char kSvcUuid[]    = "41470001-7a13-4b1e-9c2f-1d0e5f6a7b8c";
constexpr char kCfgUuid[]    = "41470002-7a13-4b1e-9c2f-1d0e5f6a7b8c";
constexpr char kTeleUuid[]   = "41470003-7a13-4b1e-9c2f-1d0e5f6a7b8c";
constexpr char kCmdUuid[]    = "41470004-7a13-4b1e-9c2f-1d0e5f6a7b8c";
constexpr char kStatusUuid[] = "41470005-7a13-4b1e-9c2f-1d0e5f6a7b8c";

constexpr char kNvsNamespace[] = "agcfg";
constexpr char kNvsKey[]       = "cfg";

/* Wire format v4 — cursor mix shrinks to 6 lanes (drops GY/AY/PITCH) and adds
 * wrist_roll_comp_milli. v4 layout:
 *   28 (v2 fields) + 1 (madgwick_enabled) + 2·6·2 (mix) + 2 (comp) = 55.
 * Strict: a wrong size or wrong version is a hard reject; no migration. */
constexpr uint8_t kConfigVersion = 4;
constexpr size_t  kConfigSize    = 55;
constexpr size_t  kTelemetrySize = 24;
constexpr size_t  kStatusSize    = 4;

/* Defaults: cursor X from compensated YAW (with a small raw GZ feed-forward
 * for snap); cursor Y from compensated ROLL (with raw GX feed-forward).
 * GY/AY/PITCH are not present — they're wrist-twist signals, not gesture.
 * Wrist-roll compensation defaults to full (1000 = 1.0) so the cursor mapping
 * is invariant to wrist orientation out of the box. Signal order matches
 * AG_MIX_*: { GX, GZ, AX, AZ, ROLL, YAW }. */
const dd_ble_cfg_t kBuiltinDefaults = {
    /* sens_x_milli            */ 1000,
    /* sens_y_milli            */ 1000,
    /* deadzone_mrad           */ 15,
    /* madgwick_beta_milli     */ 145,
    /* debounce_ms             */ 15,
    /* touch_threshold[]       */ {20, 20, 20, 20},
    /* click_action[] — slot order [PINKY, INDEX, RING, MIDDLE] */ {
        AG_CLICK_NONE,
        AG_CLICK_LEFT,
        AG_CLICK_SCROLL_MODE,
        AG_CLICK_RIGHT,
    },
    /* modifier_pad            */ AG_NO_MODIFIER,
    /* click_action_alt[]      */ {AG_CLICK_NONE, AG_CLICK_NONE, AG_CLICK_NONE},
    /* madgwick_enabled        */ 1,
    /* mix_x_milli[]           */ {   0,  -50, 0, 0,     0, -1000 },
    /* mix_y_milli[]           */ { +50,    0, 0, 0, +1000,     0 },
    /* wrist_roll_comp_milli   */ 1000,
};

static dd_ble_cfg_t   s_cfg              = kBuiltinDefaults;
static volatile uint32_t s_version       = 1;
static volatile uint8_t  s_pending       = DD_BLE_CFG_CMD_NONE;
static uint8_t        s_seq              = 0;
static bool           s_inited           = false;
/* Set/cleared from the NimBLE host task via the telemetry CCC callback.
 * Single-byte volatile is atomic on ESP32 (Xtensa); no portMUX needed. */
static volatile bool  s_tele_subscribed  = false;
static portMUX_TYPE   s_mux              = portMUX_INITIALIZER_UNLOCKED;

static NimBLECharacteristic *s_config = nullptr;
static NimBLECharacteristic *s_tele   = nullptr;
static NimBLECharacteristic *s_cmd    = nullptr;
static NimBLECharacteristic *s_status = nullptr;

static inline void put_u16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)(v >> 8);
}
static inline uint16_t get_u16(const uint8_t *p) {
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}
static inline void put_i16(uint8_t *p, int16_t v) {
    put_u16(p, (uint16_t)v);
}
static inline int16_t get_i16(const uint8_t *p) {
    return (int16_t)get_u16(p);
}

/* Clamp helpers. */
static inline uint16_t clamp_u16(uint16_t v, uint16_t lo, uint16_t hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}
static inline int16_t clamp_i16(int16_t v, int16_t lo, int16_t hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}
static inline uint8_t clamp_action(uint8_t a) {
    return (a > AG_CLICK_MAX) ? AG_CLICK_NONE : a;
}

/* Wire layout v4 (55 bytes). Offsets [0..28] unchanged from v3. Tail:
 *   [29..40]  mix_x_milli[6]            6 × i16 little-endian
 *   [41..52]  mix_y_milli[6]            6 × i16 little-endian
 *   [53..54]  wrist_roll_comp_milli     u16
 */
static void encode_config(uint8_t buf[kConfigSize], const dd_ble_cfg_t *c) {
    buf[0] = kConfigVersion;
    buf[1] = 0;                                      /* flags (clean)       */
    put_u16(&buf[2],  c->sens_x_milli);
    put_u16(&buf[4],  c->sens_y_milli);
    put_u16(&buf[6],  c->deadzone_mrad);
    put_u16(&buf[8],  c->madgwick_beta_milli);
    put_u16(&buf[10], c->debounce_ms);
    for (int i = 0; i < 4; ++i) put_u16(&buf[12 + i * 2], c->touch_threshold[i]);
    for (int i = 0; i < 4; ++i) buf[20 + i] = c->click_action[i];
    buf[24] = c->modifier_pad;
    for (int i = 0; i < 3; ++i) buf[25 + i] = c->click_action_alt[i];
    buf[28] = c->madgwick_enabled ? 1 : 0;
    for (int i = 0; i < AG_MIX_COUNT; ++i) put_i16(&buf[29 + i * 2], c->mix_x_milli[i]);
    for (int i = 0; i < AG_MIX_COUNT; ++i) put_i16(&buf[41 + i * 2], c->mix_y_milli[i]);
    put_u16(&buf[53], c->wrist_roll_comp_milli);
}

/* Parse, version-check, and clamp a v4 (55-byte) config blob.
 * No fallback: a wrong size or wrong version is a hard reject. */
static bool decode_config(dd_ble_cfg_t *c, const uint8_t *p, size_t n) {
    if (n < kConfigSize)        return false;
    if (p[0] != kConfigVersion) return false;

    c->sens_x_milli        = clamp_u16(get_u16(&p[2]),  100, 5000);
    c->sens_y_milli        = clamp_u16(get_u16(&p[4]),  100, 5000);
    c->deadzone_mrad       = clamp_u16(get_u16(&p[6]),    0, 1000);
    c->madgwick_beta_milli = clamp_u16(get_u16(&p[8]),    0, 1000);
    c->debounce_ms         = clamp_u16(get_u16(&p[10]),   5,  200);
    for (int i = 0; i < 4; ++i) {
        c->touch_threshold[i] = clamp_u16(get_u16(&p[12 + i * 2]), 1, 4095);
    }
    for (int i = 0; i < 4; ++i) c->click_action[i] = clamp_action(p[20 + i]);
    uint8_t mod = p[24];
    c->modifier_pad = (mod == AG_NO_MODIFIER || mod < 4) ? mod : AG_NO_MODIFIER;
    for (int i = 0; i < 3; ++i) {
        uint8_t a = clamp_action(p[25 + i]);
        /* AG_CLICK_CLUTCH / AG_CLICK_SCROLL_MODE are hold-modal: forbid in alt
         * to keep modifier+alt semantics simple. */
        if (a == AG_CLICK_CLUTCH || a == AG_CLICK_SCROLL_MODE) a = AG_CLICK_NONE;
        c->click_action_alt[i] = a;
    }
    c->madgwick_enabled = (p[28] != 0) ? 1 : 0;
    for (int i = 0; i < AG_MIX_COUNT; ++i) {
        c->mix_x_milli[i] = clamp_i16(get_i16(&p[29 + i * 2]), -2000, +2000);
        c->mix_y_milli[i] = clamp_i16(get_i16(&p[41 + i * 2]), -2000, +2000);
    }
    c->wrist_roll_comp_milli = clamp_u16(get_u16(&p[53]), 0, 1000);
    return true;
}

static void seed_config_characteristic(void) {
    uint8_t buf[kConfigSize];
    portENTER_CRITICAL(&s_mux);
    encode_config(buf, &s_cfg);
    portEXIT_CRITICAL(&s_mux);
    if (s_config) s_config->setValue(buf, kConfigSize);
}

class ConfigCallbacks : public NimBLECharacteristicCallbacks {
public:
    void onWrite(NimBLECharacteristic *c) override {
        NimBLEAttValue v = c->getValue();
        dd_ble_cfg_t parsed;
        if (!decode_config(&parsed, v.data(), v.length())) {
            printf("[dd_ble_cfg] config write too short (%u bytes) — ignored\n",
                   (unsigned)v.length());
            return;
        }
        portENTER_CRITICAL(&s_mux);
        s_cfg = parsed;
        s_version++;
        portEXIT_CRITICAL(&s_mux);
        /* Re-publish a canonical (clean-flag) value so reads are consistent. */
        seed_config_characteristic();
        printf("[dd_ble_cfg] config v4: sensX=%u sensY=%u dz=%umrad "
               "beta=%u debounce=%ums mod=%u madgwick=%u wrist_comp=%u "
               "click=[%u,%u,%u,%u] alt=[%u,%u,%u]\n",
               parsed.sens_x_milli, parsed.sens_y_milli,
               parsed.deadzone_mrad, parsed.madgwick_beta_milli,
               parsed.debounce_ms, (unsigned)parsed.modifier_pad,
               (unsigned)parsed.madgwick_enabled,
               (unsigned)parsed.wrist_roll_comp_milli,
               parsed.click_action[0], parsed.click_action[1],
               parsed.click_action[2], parsed.click_action[3],
               parsed.click_action_alt[0], parsed.click_action_alt[1],
               parsed.click_action_alt[2]);
    }
};

class CommandCallbacks : public NimBLECharacteristicCallbacks {
public:
    void onWrite(NimBLECharacteristic *c) override {
        NimBLEAttValue v = c->getValue();
        if (v.length() < 1) return;
        s_pending = v.data()[0];
        printf("[dd_ble_cfg] command opcode=0x%02X\n", (unsigned)s_pending);
    }
};

/* Telemetry CCC tracker. Telemetry is heavy (24 bytes @ several Hz) and is
 * only useful when the companion app is open. Without this gate we spend BLE
 * connection-event slots on notifications nobody reads, starving the HID
 * input-report path and dragging cursor responsiveness on the host side. */
class TelemetrySubCallbacks : public NimBLECharacteristicCallbacks {
public:
    void onSubscribe(NimBLECharacteristic * /*pChar*/,
                     ble_gap_conn_desc   * /*desc*/,
                     uint16_t             subValue) override {
        const bool on = (subValue & 0x0001) != 0;
        s_tele_subscribed = on;
        printf("[dd_ble_cfg] telemetry %s\n", on ? "subscribed" : "unsubscribed");
    }
};

static ConfigCallbacks        s_config_cb;
static CommandCallbacks       s_command_cb;
static TelemetrySubCallbacks  s_tele_cb;

} /* namespace */

extern "C" ag_result_t dd_ble_cfg_init(const dd_ble_cfg_t *defaults) {
    if (s_inited) return AG_OK;

    /* 1. Decide initial config: NVS if present, else caller defaults, else built-in. */
    s_cfg = defaults ? *defaults : kBuiltinDefaults;
    {
        Preferences prefs;
        if (prefs.begin(kNvsNamespace, /*readOnly=*/true)) {
            uint8_t buf[kConfigSize];
            size_t got = prefs.getBytes(kNvsKey, buf, sizeof(buf));
            if (got == kConfigSize) {
                dd_ble_cfg_t loaded;
                if (decode_config(&loaded, buf, got)) {
                    s_cfg = loaded;
                    printf("[dd_ble_cfg] loaded config from NVS\n");
                }
            }
            prefs.end();
        }
    }

    /* 2. Attach to the existing NimBLE server (dd_ble_hid created it). */
    NimBLEServer *server = NimBLEDevice::getServer();
    if (server == nullptr) {
        printf("[dd_ble_cfg] no NimBLE server — call dd_ble_hid_init_server first\n");
        return AG_ERR_STATE;
    }

    /* Caller (app_controller) must invoke this between dd_ble_hid_init_server()
     * and dd_ble_hid_start(). Registering after dd_ble_hid_start() puts our
     * service in the NimBLE-Arduino object tree but NOT in the underlying att
     * table — BlueZ then discovers the service shell with zero characteristics
     * and Web Bluetooth fails at getCharacteristic. */
    NimBLEService *svc = server->createService(kSvcUuid);
    if (svc == nullptr) return AG_ERR_INIT;

    s_config = svc->createCharacteristic(
        kCfgUuid, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
    s_tele = svc->createCharacteristic(
        kTeleUuid, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    s_cmd = svc->createCharacteristic(
        kCmdUuid, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    s_status = svc->createCharacteristic(
        kStatusUuid, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);

    if (!s_config || !s_tele || !s_cmd || !s_status) return AG_ERR_INIT;

    s_config->setCallbacks(&s_config_cb);
    s_cmd->setCallbacks(&s_command_cb);
    s_tele->setCallbacks(&s_tele_cb);

    seed_config_characteristic();

    uint8_t st[kStatusSize] = {0, DD_BLE_CFG_ST_IDLE, 0, 0};
    s_status->setValue(st, sizeof(st));

    svc->start();

    s_inited = true;
    printf("[dd_ble_cfg] config service registered\n");
    return AG_OK;
}

extern "C" void dd_ble_cfg_get_config(dd_ble_cfg_t *out) {
    if (out == nullptr) return;
    portENTER_CRITICAL(&s_mux);
    *out = s_cfg;
    portEXIT_CRITICAL(&s_mux);
}

extern "C" uint32_t dd_ble_cfg_config_version(void) {
    return s_version;
}

extern "C" void dd_ble_cfg_publish_telemetry(const dd_ble_cfg_telemetry_t *t) {
    if (t == nullptr || s_tele == nullptr) return;
    /* No companion app listening → skip the encode + notify entirely. The
     * NimBLE notify() would silently drop with no subscribers, but it still
     * costs time on the host task. Pre-flighting it here keeps BLE airtime
     * free for the HID input-report path. */
    if (!s_tele_subscribed) return;

    uint8_t buf[kTelemetrySize];
    buf[0] = 1;            /* version */
    buf[1] = s_seq++;      /* sequence, wraps at 256 */
    for (int i = 0; i < 3; ++i) put_u16(&buf[2 + i * 2], (uint16_t)t->accel_mg[i]);
    for (int i = 0; i < 3; ++i) put_u16(&buf[8 + i * 2], (uint16_t)t->gyro_mdps[i]);
    for (int i = 0; i < 4; ++i) put_u16(&buf[14 + i * 2], t->touch[i]);
    buf[22] = t->battery_pct;
    buf[23] = t->flags;

    s_tele->setValue(buf, sizeof(buf));
    s_tele->notify();
}

extern "C" uint8_t dd_ble_cfg_take_command(void) {
    uint8_t op = s_pending;
    if (op != DD_BLE_CFG_CMD_NONE) s_pending = DD_BLE_CFG_CMD_NONE;
    return op;
}

extern "C" void dd_ble_cfg_set_status(uint8_t opcode, uint8_t state, uint8_t progress) {
    if (s_status == nullptr) return;
    uint8_t st[kStatusSize] = {opcode, state, progress, 0};
    s_status->setValue(st, sizeof(st));
    s_status->notify();
}

extern "C" ag_result_t dd_ble_cfg_save(void) {
    uint8_t buf[kConfigSize];
    portENTER_CRITICAL(&s_mux);
    encode_config(buf, &s_cfg);
    portEXIT_CRITICAL(&s_mux);

    Preferences prefs;
    if (!prefs.begin(kNvsNamespace, /*readOnly=*/false)) return AG_ERR_IO;
    size_t wrote = prefs.putBytes(kNvsKey, buf, sizeof(buf));
    prefs.end();
    if (wrote != kConfigSize) return AG_ERR_IO;
    printf("[dd_ble_cfg] config saved to NVS\n");
    return AG_OK;
}

extern "C" void dd_ble_cfg_factory_reset(void) {
    portENTER_CRITICAL(&s_mux);
    s_cfg = kBuiltinDefaults;
    s_version++;
    portEXIT_CRITICAL(&s_mux);

    Preferences prefs;
    if (prefs.begin(kNvsNamespace, /*readOnly=*/false)) {
        prefs.remove(kNvsKey);
        prefs.end();
    }
    seed_config_characteristic();
    printf("[dd_ble_cfg] factory reset\n");
}
