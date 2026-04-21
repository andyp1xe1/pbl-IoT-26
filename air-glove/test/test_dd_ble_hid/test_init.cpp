/* Unity tests for dd_ble_hid. Runs on target (env:esp32dev) only; excluded
 * from env:native via `test_ignore = test_dd_*` because it pulls in NimBLE.
 *
 * No BLE host required — these tests only exercise the local driver state
 * machine: init succeeds, "connected" starts false, and send-while-
 * disconnected is a silent no-op (returns AG_OK per the contract).
 *
 * Run with:
 *     pio test -e esp32dev -f test_dd_ble_hid
 *
 * Manual HIL tests (TC-FR01-01, TC-FR02-01, TC-NFR-HID-001) live in
 * docs/plans/09-integration-and-bringup.md — they need real hosts and
 * are not automatable.
 */

#include <Arduino.h>
#include <unity.h>

#include "dd_ble_hid.h"

void setUp(void) {}
void tearDown(void) {}

static void test_init_returns_ok(void) {
    TEST_ASSERT_EQUAL_INT(AG_OK, dd_ble_hid_init("AirGloveTest"));
}

static void test_init_is_idempotent(void) {
    /* Calling init twice must not crash; second call returns AG_OK
     * and leaves the previously-configured stack alone. */
    TEST_ASSERT_EQUAL_INT(AG_OK, dd_ble_hid_init("AirGloveTest"));
}

static void test_not_connected_at_boot(void) {
    TEST_ASSERT_FALSE(dd_ble_hid_is_connected());
}

static void test_send_with_null_is_arg_error(void) {
    TEST_ASSERT_EQUAL_INT(AG_ERR_ARG, dd_ble_hid_send(nullptr));
}

static void test_send_when_disconnected_is_ok_noop(void) {
    const hid_mouse_report_t zero = { 0, 0, 0, 0 };
    TEST_ASSERT_EQUAL_INT(AG_OK, dd_ble_hid_send(&zero));
    /* Still not connected — no side-effects. */
    TEST_ASSERT_FALSE(dd_ble_hid_is_connected());
}

void setup(void) {
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_init_returns_ok);
    RUN_TEST(test_init_is_idempotent);
    RUN_TEST(test_not_connected_at_boot);
    RUN_TEST(test_send_with_null_is_arg_error);
    RUN_TEST(test_send_when_disconnected_is_ok_noop);
    UNITY_END();
}

void loop(void) {}
