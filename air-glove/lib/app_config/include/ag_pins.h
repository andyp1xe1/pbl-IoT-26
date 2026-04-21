#ifndef AG_PINS_H
#define AG_PINS_H

/* I2C pins for the MPU6050. Matches docs/srs/hardware.md. */
#define AG_PIN_I2C_SDA   21
#define AG_PIN_I2C_SCL   22

/* Capacitive touch channel numbers (ESP32 touch_pad_t indices).
 * Physical GPIOs in parentheses. Thumb is the common reference. */
#define AG_TOUCH_THUMB   0   /* T0 / GPIO4  */
#define AG_TOUCH_INDEX   2   /* T2 / GPIO2  */
#define AG_TOUCH_MIDDLE  3   /* T3 / GPIO15 */
#define AG_TOUCH_RING    4   /* T4 / GPIO13 */

#endif /* AG_PINS_H */
