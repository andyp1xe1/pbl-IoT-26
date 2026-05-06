#ifndef AG_PINS_H
#define AG_PINS_H

/* I2C pins for the MPU6050. Matches docs/srs/hardware.md. */
#define AG_PIN_I2C_SDA   21
#define AG_PIN_I2C_SCL   22

/* Finger input pin assignments.
 *
 * THUMB  — capacitive touch pad (ESP32 T0, GPIO4). Bare electrode/wire.
 *          Keep free in air during boot so calibration gets a clean baseline.
 *
 * INDEX  — tactile push-button, one leg to GPIO14, other leg to GND.
 * MIDDLE — tactile push-button, one leg to GPIO15, other leg to GND.
 * RING   — tactile push-button, one leg to GPIO13, other leg to GND.
 *          All three use INPUT_PULLUP; the ESP32's internal ~45 kΩ pull-up
 *          keeps the line HIGH when the button is open. No external resistor
 *          needed. */
#define AG_TOUCH_THUMB   0   /* T0  / GPIO4  — capacitive             */
#define AG_TOUCH_INDEX   6   /* T6  / GPIO14 — button (INPUT_PULLUP)  */
#define AG_TOUCH_MIDDLE  3   /* T3  / GPIO15 — button (INPUT_PULLUP)  */
#define AG_TOUCH_RING    4   /* T4  / GPIO13 — button (INPUT_PULLUP)  */

#endif /* AG_PINS_H */
