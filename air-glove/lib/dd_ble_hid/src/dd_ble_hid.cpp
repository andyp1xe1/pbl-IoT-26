/* dd_ble_hid — stub until plan 04 lands. NimBLE-Arduino will be added
 * as a library dependency and wired through here. */

#include "dd_ble_hid.h"

extern "C" ag_result_t dd_ble_hid_init(const char *device_name) {
    (void)device_name;
    return AG_ERR_INIT;
}

extern "C" ag_result_t dd_ble_hid_send(const hid_mouse_report_t *r) {
    (void)r;
    return AG_ERR_STATE;
}

extern "C" bool dd_ble_hid_is_connected(void) {
    return false;
}
