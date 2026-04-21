/* dd_ble_hid — NimBLE-Arduino BLE HID mouse driver (Phase I).
 *
 * ADR-004 locks in NimBLE-Arduino as the BLE stack. ADR-005 permits the
 * NimBLE includes below because this is a dd_* lib. The public header
 * (`include/dd_ble_hid.h`) exposes only the C interface defined in E02
 * — no NimBLE type ever leaks across the lib boundary.
 *
 * NimBLE-Arduino API surface is 1.x here (h2zero/NimBLE-Arduino ^1.4.0).
 * A minor API tweak may be required on a future 2.x bump — see the
 * "API version sensitivity" note in plan 04.
 *
 * All module state is file-scope `static`; only the three public
 * `extern "C"` entry points are exported.
 */

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <NimBLEHIDDevice.h>
#include <NimBLEServer.h>
#include <NimBLEAdvertising.h>
#include <esp_timer.h>
#include <stdio.h>
#include <string.h>

#include "dd_ble_hid.h"
#include "ble_hid_report_map.h"

namespace {

/* HID "Appearance" for a mouse, per Bluetooth Core appearance values.  */
static constexpr uint16_t kAppearanceMouse = 0x03C2;

/* Connection-parameter hints passed on connect. Interval units are
 * 1.25 ms; supervision-timeout units are 10 ms.
 *   min=6  → 7.5 ms   (host may round up)
 *   max=12 → 15 ms    (target cap for low latency)
 *   latency=0         (no slave latency — we want quick reports)
 *   timeout=200       (2000 ms supervision) */
static constexpr uint16_t kConnIntervalMinUnits = 6;
static constexpr uint16_t kConnIntervalMaxUnits = 12;
static constexpr uint16_t kConnSlaveLatency     = 0;
static constexpr uint16_t kConnSupervisionTimeoutUnits = 200;

static volatile bool      s_connected = false;
static bool               s_initialized = false;
static NimBLEServer      *s_server   = nullptr;
static NimBLEHIDDevice   *s_hid      = nullptr;
static NimBLECharacteristic *s_input = nullptr;

/* Connection callback. NimBLE invokes these on its own internal task;
 * writes to `s_connected` are a single-byte volatile store, safe to
 * read lock-free from other tasks on ESP32. */
class ServerCallbacks : public NimBLEServerCallbacks {
public:
    void onConnect(NimBLEServer *server, ble_gap_conn_desc *desc) override {
        s_connected = true;
        /* Negotiate a tight interval for low cursor latency. Host may
         * clamp the request but will never go below its minimum. */
        server->updateConnParams(desc->conn_handle,
                                 kConnIntervalMinUnits,
                                 kConnIntervalMaxUnits,
                                 kConnSlaveLatency,
                                 kConnSupervisionTimeoutUnits);
        printf("[dd_ble_hid] connected\n");
    }
    void onDisconnect(NimBLEServer *server) override {
        (void)server;
        s_connected = false;
        printf("[dd_ble_hid] disconnected; re-advertising\n");
        NimBLEDevice::startAdvertising();
    }
};

static ServerCallbacks s_server_cb;

} /* namespace */

extern "C" ag_result_t dd_ble_hid_init(const char *device_name) {
    if (s_initialized) return AG_OK;

    const char *name = (device_name && device_name[0]) ? device_name : "AirGlove";

    NimBLEDevice::init(name);

    /* Just-works bonding: bonding=true + MITM=false + SC=true. MITM would
     * require a PIN display the glove does not have. IOCap advertises our
     * lack of I/O and forces just-works pairing across Windows/Linux/macOS. */
    NimBLEDevice::setSecurityAuth(true, false, true);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);

    s_server = NimBLEDevice::createServer();
    if (s_server == nullptr) return AG_ERR_INIT;
    s_server->setCallbacks(&s_server_cb);

    s_hid = new NimBLEHIDDevice(s_server);
    if (s_hid == nullptr) return AG_ERR_INIT;

    s_hid->manufacturer()->setValue("AirGlove");
    /* PnP ID: vendor-source=USB-IF (0x02), placeholder VID/PID/version.
     * Replace with real IDs if this ships as a product; the device still
     * enumerates as a mouse with the placeholders. */
    s_hid->pnp(0x02, /*vid*/0xE502, /*pid*/0xA111, /*ver*/0x0210);
    /* HID info: country=0 (not localised), flags=0x01 remote wake. */
    s_hid->hidInfo(0x00, 0x01);

    s_hid->reportMap((uint8_t *)kHidReportMap, sizeof(kHidReportMap));

    /* inputReport(0) matches our descriptor's absence of a Report ID — the
     * argument is the NimBLE report-characteristic index, not a wire ID. */
    s_input = s_hid->inputReport(0);
    if (s_input == nullptr) return AG_ERR_INIT;

    s_hid->startServices();

    NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
    adv->setAppearance(kAppearanceMouse);
    adv->addServiceUUID(s_hid->hidService()->getUUID());
    adv->setScanResponse(true);
    adv->start();

    s_initialized = true;
    printf("[dd_ble_hid] advertising: %s\n", name);
    return AG_OK;
}

extern "C" ag_result_t dd_ble_hid_send(const hid_mouse_report_t *r) {
    if (r == nullptr)        return AG_ERR_ARG;
    if (!s_initialized)      return AG_ERR_STATE;
    if (!s_connected)        return AG_OK;              /* silent drop */
    if (s_input == nullptr)  return AG_ERR_STATE;

    /* Wire layout MUST match the report-map in `ble_hid_report_map.h`. */
    uint8_t buf[4];
    buf[0] = (uint8_t)(r->buttons & 0x07);     /* low 3 bits = L/R/M  */
    buf[1] = (uint8_t)r->dx;
    buf[2] = (uint8_t)r->dy;
    buf[3] = (uint8_t)r->wheel;

    s_input->setValue(buf, sizeof(buf));
    s_input->notify();
    return AG_OK;
}

extern "C" bool dd_ble_hid_is_connected(void) {
    return s_connected;
}
