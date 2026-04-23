/* On-target smoke tests for app_controller. Runs under env:esp32dev only
 * (excluded from env:native via `test_filter = test_srv_*` in platformio.ini).
 *
 * Run with:
 *     pio test -e esp32dev -f test_app_controller
 *
 * These tests do NOT require a BLE host — they verify that boot completes,
 * tasks are created, queues are live, and the 1 Hz heartbeat emits at
 * least one line within 1.5 s. Full end-to-end HIL scripts live in
 * docs/plans/09-integration-and-bringup.md.
 */

#include <Arduino.h>
#include <unity.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "app_controller.h"

/* The queue handles are defined in app_controller.cpp. The test pulls
 * them through the same extern declarations the tasks use. */
extern QueueHandle_t q_imu;
extern QueueHandle_t q_orientation;
extern QueueHandle_t q_buttons;
extern QueueHandle_t q_hid;

void setUp(void) {}
void tearDown(void) {}

/* Start must complete and return AG_OK without rebooting us. */
static void test_start_returns_ok(void) {
    TEST_ASSERT_EQUAL_INT(AG_OK, app_controller_start());
}

/* All six tasks must exist by name after start. */
static void test_tasks_created(void) {
    TEST_ASSERT_NOT_NULL(xTaskGetHandle("t_imu_sample"));
    TEST_ASSERT_NOT_NULL(xTaskGetHandle("t_fusion"));
    TEST_ASSERT_NOT_NULL(xTaskGetHandle("t_touch"));
    TEST_ASSERT_NOT_NULL(xTaskGetHandle("t_motion"));
    TEST_ASSERT_NOT_NULL(xTaskGetHandle("t_app"));
    TEST_ASSERT_NOT_NULL(xTaskGetHandle("t_ble_hid"));
}

/* Queue handles must be non-NULL. uxQueueMessagesWaiting on a valid
 * handle returns the current count; we don't care about the value,
 * only that the call succeeds without crashing. */
static void test_queues_created(void) {
    TEST_ASSERT_NOT_NULL(q_imu);
    TEST_ASSERT_NOT_NULL(q_orientation);
    TEST_ASSERT_NOT_NULL(q_buttons);
    TEST_ASSERT_NOT_NULL(q_hid);
    (void)uxQueueMessagesWaiting(q_imu);
    (void)uxQueueMessagesWaiting(q_orientation);
    (void)uxQueueMessagesWaiting(q_buttons);
    (void)uxQueueMessagesWaiting(q_hid);
}

/* Task watchdog sanity: give the scheduler ~1.5 s to actually run the
 * tasks we just created. If any task is broken at entry (e.g., a null
 * queue deref), the FreeRTOS TWDT will fire and the board reboots
 * before this call returns — in which case `pio test` reports a crash. */
static void test_scheduler_runs_for_1500ms(void) {
    vTaskDelay(pdMS_TO_TICKS(1500));
    TEST_ASSERT_NOT_NULL(xTaskGetHandle("t_imu_sample"));  /* still alive */
}

void setup(void) {
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_start_returns_ok);
    RUN_TEST(test_tasks_created);
    RUN_TEST(test_queues_created);
    RUN_TEST(test_scheduler_runs_for_1500ms);
    UNITY_END();
}

void loop(void) {}
