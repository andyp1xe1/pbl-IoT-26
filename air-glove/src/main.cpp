#include <stdio.h>
#include "app_controller.h"

void setup(void) {
    printf("[boot] AirGlove starting\n");
    if (app_controller_start() != AG_OK) {
        printf("[boot] app_controller_start failed\n");
    }
}

void loop(void) {
    /* App runs in FreeRTOS tasks; nothing to do here. */
}
