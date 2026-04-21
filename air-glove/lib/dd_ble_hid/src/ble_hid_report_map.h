/* ble_hid_report_map.h — standard 4-byte BLE HID mouse descriptor.
 *
 * Layout (no Report ID):
 *   byte 0 = buttons (bit 0 = L, bit 1 = R, bit 2 = M, bits 3-7 = pad)
 *   byte 1 = dx    (int8, relative X)
 *   byte 2 = dy    (int8, relative Y)
 *   byte 3 = wheel (int8, relative vertical scroll)
 *
 * Private to dd_ble_hid; not installed in include/.
 */

#ifndef BLE_HID_REPORT_MAP_H
#define BLE_HID_REPORT_MAP_H

#include <stdint.h>

static const uint8_t kHidReportMap[] = {
    0x05, 0x01,        /* Usage Page (Generic Desktop)             */
    0x09, 0x02,        /*   Usage (Mouse)                          */
    0xA1, 0x01,        /*   Collection (Application)               */
    0x09, 0x01,        /*     Usage (Pointer)                      */
    0xA1, 0x00,        /*     Collection (Physical)                */
    /* --- Buttons: 3 bits + 5-bit padding ---------------------- */
    0x05, 0x09,        /*       Usage Page (Button)                */
    0x19, 0x01,        /*       Usage Minimum (Button 1)           */
    0x29, 0x03,        /*       Usage Maximum (Button 3)           */
    0x15, 0x00,        /*       Logical Min (0)                    */
    0x25, 0x01,        /*       Logical Max (1)                    */
    0x95, 0x03,        /*       Report Count (3)                   */
    0x75, 0x01,        /*       Report Size (1)                    */
    0x81, 0x02,        /*       Input (Data, Var, Abs)             */
    0x95, 0x01,        /*       Report Count (1)                   */
    0x75, 0x05,        /*       Report Size (5)                    */
    0x81, 0x03,        /*       Input (Const)  padding             */
    /* --- X, Y, Wheel: three int8 axes --------------------------- */
    0x05, 0x01,        /*       Usage Page (Generic Desktop)       */
    0x09, 0x30,        /*       Usage (X)                          */
    0x09, 0x31,        /*       Usage (Y)                          */
    0x09, 0x38,        /*       Usage (Wheel)                      */
    0x15, 0x81,        /*       Logical Min (-127)                 */
    0x25, 0x7F,        /*       Logical Max ( 127)                 */
    0x75, 0x08,        /*       Report Size (8)                    */
    0x95, 0x03,        /*       Report Count (3)                   */
    0x81, 0x06,        /*       Input (Data, Var, Rel)             */
    0xC0,              /*     End Collection                       */
    0xC0               /*   End Collection                         */
};

#endif /* BLE_HID_REPORT_MAP_H */
