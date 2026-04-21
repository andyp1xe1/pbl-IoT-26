# E02 — HW Abstraction Layer

- **Status:** Not Started
- **Phase:** I
- **Owns:** `air-glove/lib/dd_mpu6050/include/`, `lib/dd_touch/include/`, `lib/dd_ble_hid/include/`, shared types in `lib/app_config/include/`
- **Plan:** `docs/plans/01-scaffolding.md` §"Interface contracts"
- **Realises:** NFR-MOD-001

## Goal

Define the C-style public headers for every `dd_*` driver and the shared data types (`imu_sample_t`, `touch_sample_t`, `hid_mouse_report_t`, error codes). Interfaces only — implementations come in E03–E05. Services (`srv_*`) must compile against these headers in the `native` env using stub implementations.

## Scope

**In:**
- `app_config/include/ag_types.h` — shared POD types and result codes used across the app.
- `dd_mpu6050/include/dd_mpu6050.h` — interface for E03.
- `dd_touch/include/dd_touch.h` — interface for E04.
- `dd_ble_hid/include/dd_ble_hid.h` — interface for E05.
- Native-env stub `.cpp` files (empty implementations returning OK) so `srv_*` tests can link.

**Out:**
- Any hardware access logic.

## Public interface (sketch)

```c
// app_config/include/ag_types.h
typedef int ag_result_t;              // 0 = OK, <0 = error
#define AG_OK 0
#define AG_ERR_IO   -1
#define AG_ERR_INIT -2
#define AG_ERR_ARG  -3

typedef struct { float ax, ay, az; float gx, gy, gz; uint64_t t_us; } imu_sample_t;

typedef enum {
  TOUCH_PAD_THUMB = 0, TOUCH_PAD_INDEX, TOUCH_PAD_MIDDLE, TOUCH_PAD_RING,
  TOUCH_PAD_COUNT
} touch_pad_id_t;
typedef struct { uint16_t raw[TOUCH_PAD_COUNT]; uint8_t touched_mask; uint64_t t_us; } touch_sample_t;

typedef struct { int8_t dx, dy; uint8_t buttons; int8_t wheel; } hid_mouse_report_t;
```

```c
// dd_mpu6050.h
ag_result_t dd_mpu6050_init(void);
ag_result_t dd_mpu6050_read(imu_sample_t *out);

// dd_touch.h
ag_result_t dd_touch_init(void);
ag_result_t dd_touch_read(touch_sample_t *out);

// dd_ble_hid.h
ag_result_t dd_ble_hid_init(const char *device_name);
ag_result_t dd_ble_hid_send(const hid_mouse_report_t *r);
bool        dd_ble_hid_is_connected(void);
```

## Acceptance criteria

- [ ] All four headers compile stand-alone (`-Wall -Werror`, no Arduino include).
- [ ] `srv_*` libs can include these headers and compile under `env:native`.
- [ ] Every type and function documented with a one-line comment on its contract (units, ownership, thread-safety).

## Dependencies

- E01.

## Progress log

- 2026-04-21: Epic created. Contract surface drafted above; final signatures finalised in `docs/plans/02`–`04`.
