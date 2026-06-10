#ifndef DD_BLE_HID_H
#define DD_BLE_HID_H

#include "ag_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Phase 1: bring up the BLE stack, create the server, register the HID
 * service objects, but do NOT start any service and do NOT advertise.
 * After this call, NimBLEDevice::getServer() is valid and other drivers
 * may register their own services on it (createService + createCharacteristic
 * + start). */
ag_result_t dd_ble_hid_init_server(const char *device_name);

/* Phase 2: finalise the HID services (att table) and start advertising.
 * Must be called exactly once, after every other service has been
 * registered. Safe to call only after dd_ble_hid_init_server(). */
ag_result_t dd_ble_hid_start(void);

/* Send one mouse report. Non-blocking; returns AG_ERR_STATE when not
 * connected, AG_OK on notify enqueued. May be called at up to 125 Hz. */
ag_result_t dd_ble_hid_send(const hid_mouse_report_t *r);

/* True while a host is currently connected and has enabled notifications
 * on the HID input-report characteristic. */
bool dd_ble_hid_is_connected(void);

#ifdef __cplusplus
}
#endif
#endif /* DD_BLE_HID_H */
