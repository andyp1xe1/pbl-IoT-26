/* dd_ble_hid.h — public interface of the BLE HID mouse driver.
 *
 * Layer: dd (device driver). Implementation wraps NimBLE-Arduino; no
 * NimBLE types leak through this header, so callers compile against it
 * in `env:native` for unit tests (stub implementation only).
 *
 * Implementation lives in plan 04 (`docs/plans/04-dd-ble-hid.md`).
 */

#ifndef DD_BLE_HID_H
#define DD_BLE_HID_H

#include "ag_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Initialise the BLE stack, register the HID mouse service and report
 * descriptor, and start advertising. `device_name` is advertised as the
 * local name (typically "AirGlove"); it is copied internally — the
 * caller does not need to keep the string alive after this returns.
 *
 * Not thread-safe; call once at boot. Returns AG_OK or AG_ERR_INIT. */
ag_result_t dd_ble_hid_init(const char *device_name);

/* Enqueue one HID mouse report for transmission. Non-blocking.
 *
 * Thread-safety: designed to be called from a single task (`t_ble_hid`
 * per `docs/srs/architecture.md`). Concurrent calls from multiple tasks
 * are NOT supported in Phase I.
 *
 * Returns AG_OK when the notify has been enqueued; AG_ERR_STATE when
 * the host is not currently connected; AG_ERR_ARG on NULL input. */
ag_result_t dd_ble_hid_send(const hid_mouse_report_t *r);

/* True iff a host is currently connected AND has enabled notifications
 * on the HID input-report characteristic. Safe to call from any task
 * (reads a `volatile bool` set by NimBLE connection callbacks). */
bool dd_ble_hid_is_connected(void);

#ifdef __cplusplus
}
#endif
#endif /* DD_BLE_HID_H */
