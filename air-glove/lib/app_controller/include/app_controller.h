#pragma once
/* app_controller — single entry point for Phase I firmware.
 *
 * Initialises every dd_* driver in order, brings up the srv_* services,
 * creates the four FreeRTOS queues (q_imu, q_orientation, q_buttons,
 * q_hid), spawns the six pinned tasks per docs/srs/architecture.md §4,
 * and runs the top-level INIT → PAIRING → ACTIVE FSM inside t_ble_hid.
 *
 * Thread-safety: call exactly once, from src/main.cpp `setup()`.
 * Platform:      esp32dev only — links FreeRTOS + every dd_* and srv_*.
 *
 * The public header deliberately exposes only `ag_types.h`; no FreeRTOS,
 * Arduino, or driver types leak through, so `src/main.cpp` stays
 * stdio-only per NFR-MOD-001.
 */

#include "ag_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Boot the firmware. Returns AG_OK after the six tasks are created and
 * the scheduler is free to run them. On a fatal driver-init failure the
 * function logs via printf and triggers `esp_restart()` after a 5 s
 * delay; it does not return to the caller in that case. */
ag_result_t app_controller_start(void);

#ifdef __cplusplus
}
#endif
