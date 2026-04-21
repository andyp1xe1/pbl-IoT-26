#ifndef DD_BLE_HID_H
#define DD_BLE_HID_H

#include "ag_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Initialise BLE stack, register HID mouse service, start advertising.
 * `device_name` is advertised as the local name (typically "AirGlove"). */
ag_result_t dd_ble_hid_init(const char *device_name);

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
