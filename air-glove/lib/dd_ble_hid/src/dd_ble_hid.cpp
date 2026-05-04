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

/* Connection parameters intentionally NOT sent on connect.
 * Calling updateConnParams() immediately in onConnect() causes Windows
 * and some Android hosts to drop the connection before the HID driver
 * has finished enumerating the device. The host negotiates its own
 * interval (typically 7.5–45 ms for HID), which is stable and fast
 * enough for a mouse. Re-add updateConnParams only if measured latency
 * becomes a problem after the host stack is fully set up. */

static volatile bool      s_connected = false;
static bool               s_initialized = false;
static NimBLEServer      *s_server   = nullptr;
static NimBLEHIDDevice   *s_hid      = nullptr;
static NimBLECharacteristic *s_input = nullptr;

/* Connection callback. NimBLE invokes these on its own internal task;
 * writes to `s_connected` are a single-byte volatile store, safe to
 * read lock-free from other tasks on ESP32.
 *
 * s_connected is intentionally NOT set in onConnect. The host (Windows,
 * macOS, Linux) needs 200–800 ms after onConnect to finish GATT discovery,
 * load the HID kernel driver, and write 0x0001 to the CCC descriptor to
 * enable notifications. Sending HID reports before that happens causes
 * most host BLE stacks to force a disconnection.
 * s_connected is set to true only in InputReportCallbacks::onSubscribe,
 * which fires exactly when the host enables notifications. */
class ServerCallbacks : public NimBLEServerCallbacks {
public:
    void onConnect(NimBLEServer * /*server*/, ble_gap_conn_desc *desc) override {
        const uint8_t *a = desc->peer_id_addr.val;
        printf("[dd_ble_hid] host connected  addr=%02X:%02X:%02X:%02X:%02X:%02X  handle=%u"
               " — awaiting HID enumeration\n",
               a[5], a[4], a[3], a[2], a[1], a[0], (unsigned)desc->conn_handle);
    }
    void onDisconnect(NimBLEServer *server) override {
        (void)server;
        s_connected = false;
        printf("[dd_ble_hid] host disconnected; re-advertising\n");
        NimBLEDevice::startAdvertising();
    }
};

/* Characteristic callback: fires when the host writes the CCC descriptor
 * on the HID input-report characteristic (subValue == 1 → notify enabled,
 * subValue == 0 → notify disabled).  This is the correct moment to mark
 * the link as ready for HID reports. */
class InputReportCallbacks : public NimBLECharacteristicCallbacks {
public:
    void onSubscribe(NimBLECharacteristic * /*pChar*/,
                     ble_gap_conn_desc   * /*desc*/,
                     uint16_t             subValue) override {
        if (subValue & 0x0001) {   /* bit 0 = notify */
            s_connected = true;
            printf("[dd_ble_hid] host enabled HID notifications — reports flowing\n");
        } else {
            s_connected = false;
            printf("[dd_ble_hid] host disabled HID notifications\n");
        }
    }
};

static ServerCallbacks      s_server_cb;
static InputReportCallbacks s_input_cb;

} /* namespace */

extern "C" ag_result_t dd_ble_hid_init(const char *device_name) {
    if (s_initialized) return AG_OK;

    const char *name = (device_name && device_name[0]) ? device_name : "AirGlove";

    NimBLEDevice::init(name);

    /* No bonding for MVP — bonding requires the host to cache keys that the
     * ESP32 does not persist across reboots. On Windows this causes an
     * immediate disconnect on every reconnect because Windows presents the
     * old keys and the ESP32 has none. Open (unauthenticated, no bonding)
     * is stable and perfectly fine for a mouse. */
    NimBLEDevice::setSecurityAuth(false, false, false);
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

    /* Register the subscription callback so s_connected is set only after
     * the host writes the CCC descriptor (notifications enabled). */
    s_input->setCallbacks(&s_input_cb);

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
    /* NimBLE-Arduino 1.4.x notify() is void — it silently drops the
     * notification if the host has not subscribed or the TX queue is full.
     * Back-pressure is handled upstream: t_ble_hid_fn paces sends at ≤67 Hz
     * (15 ms timeout) to stay within the typical 15 ms connection interval. */
    s_input->notify();
    return AG_OK;
}

extern "C" bool dd_ble_hid_is_connected(void) {
    return s_connected;
}
