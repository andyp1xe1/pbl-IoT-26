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

constexpr size_t kConfigSize    = 10;
constexpr size_t kTelemetrySize = 24;
constexpr size_t kStatusSize    = 4;

const dd_ble_cfg_t kBuiltinDefaults = {
    /* sens_x_milli  */ 1000,
    /* sens_y_milli  */ 1000,
    /* deadzone_mrad */ 4,
    /* click_map     */ 0,
};

static dd_ble_cfg_t   s_cfg          = kBuiltinDefaults;
static volatile uint32_t s_version   = 1;
static volatile uint8_t  s_pending    = DD_BLE_CFG_CMD_NONE;
static uint8_t        s_seq          = 0;
static bool           s_inited       = false;
static portMUX_TYPE   s_mux          = portMUX_INITIALIZER_UNLOCKED;

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

static void encode_config(uint8_t buf[kConfigSize], const dd_ble_cfg_t *c) {
    buf[0] = 1;                 /* version */
    buf[1] = 0;                 /* flags (clean) */
    put_u16(&buf[2], c->sens_x_milli);
    put_u16(&buf[4], c->sens_y_milli);
    put_u16(&buf[6], c->deadzone_mrad);
    buf[8] = c->click_map;
    buf[9] = 0;
}

/* Parse and clamp a 10-byte config blob. Returns false if too short. */
static bool decode_config(dd_ble_cfg_t *c, const uint8_t *p, size_t n) {
    if (n < kConfigSize) return false;
    uint16_t sx = get_u16(&p[2]);
    uint16_t sy = get_u16(&p[4]);
    uint16_t dz = get_u16(&p[6]);
    if (sx < 100)  sx = 100;          /* guard divide-by-near-zero downstream */
    if (sx > 5000) sx = 5000;
    if (sy < 100)  sy = 100;
    if (sy > 5000) sy = 5000;
    if (dz > 1000) dz = 1000;
    c->sens_x_milli  = sx;
    c->sens_y_milli  = sy;
    c->deadzone_mrad = dz;
    c->click_map     = (p[8] != 0) ? 1 : 0;
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
        printf("[dd_ble_cfg] config: sensX=%u sensY=%u dz=%umrad click=%u\n",
               parsed.sens_x_milli, parsed.sens_y_milli,
               parsed.deadzone_mrad, parsed.click_map);
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

static ConfigCallbacks  s_config_cb;
static CommandCallbacks s_command_cb;

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
        printf("[dd_ble_cfg] no NimBLE server — call dd_ble_hid_init first\n");
        return AG_ERR_STATE;
    }

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

    seed_config_characteristic();

    uint8_t st[kStatusSize] = {0, DD_BLE_CFG_ST_IDLE, 0, 0};
    s_status->setValue(st, sizeof(st));

    svc->start();

    s_inited = true;
    printf("[dd_ble_cfg] config service up\n");
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
