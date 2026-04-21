#ifndef APP_CONTROLLER_H
#define APP_CONTROLLER_H

#include "ag_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Entry point for the Phase I firmware. Initialises all dd_* drivers,
 * creates the FreeRTOS queues and tasks described in docs/srs/architecture.md,
 * and does NOT return (tasks run forever). */
ag_result_t app_controller_start(void);

#ifdef __cplusplus
}
#endif
#endif /* APP_CONTROLLER_H */
