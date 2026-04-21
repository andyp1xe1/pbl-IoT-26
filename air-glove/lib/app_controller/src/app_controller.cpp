/* app_controller — stub until plan 08 lands. Prints a boot marker and
 * returns so the scaffolded binary boots cleanly on target. */

#include <stdio.h>
#include "app_controller.h"

extern "C" ag_result_t app_controller_start(void) {
    printf("[app_controller] stub — plan 08 will wire the real tasks\n");
    return AG_OK;
}
