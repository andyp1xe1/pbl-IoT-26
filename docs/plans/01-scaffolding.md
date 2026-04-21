# Plan 01 — Scaffolding (PlatformIO + Lib Skeletons + Interface Contracts)

- **Epic:** E01 (Project Foundation) + E02 (HW Abstraction Layer contracts)
- **Goal:** Produce a green-building PlatformIO project under `air-glove/` with every lib folder present as a valid-but-empty library, and every `dd_*` public header filled in so `srv_*` and `app_*` can compile against them in both `env:esp32dev` and `env:native`.
- **Preconditions:** None — this is the first plan.

## Files to create

```
air-glove/
├── platformio.ini
├── .gitignore
├── src/
│   └── main.cpp                                    # stdio-only bootstrapper
├── include/
│   └── README.md                                   # marker; shared app headers if needed later
├── lib/
│   ├── dd_mpu6050/
│   │   ├── library.json
│   │   ├── include/dd_mpu6050.h
│   │   └── src/dd_mpu6050.cpp                      # stub returns AG_ERR_INIT
│   ├── dd_touch/
│   │   ├── library.json
│   │   ├── include/dd_touch.h
│   │   └── src/dd_touch.cpp
│   ├── dd_ble_hid/
│   │   ├── library.json
│   │   ├── include/dd_ble_hid.h
│   │   └── src/dd_ble_hid.cpp
│   ├── srv_fusion/
│   │   ├── library.json
│   │   ├── include/srv_fusion.h
│   │   └── src/srv_fusion.cpp                       # stub returns identity quaternion
│   ├── srv_motion/
│   │   ├── library.json
│   │   ├── include/srv_motion.h
│   │   └── src/srv_motion.cpp
│   ├── srv_input/
│   │   ├── library.json
│   │   ├── include/srv_input.h
│   │   └── src/srv_input.cpp
│   ├── app_controller/
│   │   ├── library.json
│   │   ├── include/app_controller.h
│   │   └── src/app_controller.cpp                   # stub: printf("boot\n"); return AG_OK;
│   └── app_config/
│       ├── library.json
│       └── include/
│           ├── ag_types.h                           # shared POD types + result codes
│           └── ag_pins.h                            # pin constants (GPIO21/22, T0-T4 etc.)
└── test/
    ├── test_srv_fusion/
    │   └── test_main.cpp                            # Unity dummy test, compiles in native
    ├── test_srv_motion/
    │   └── test_main.cpp
    └── test_srv_input/
        └── test_main.cpp
```

## Step-by-step

### 1. `platformio.ini`

Two environments. Keep library dependencies minimal — only `dd_ble_hid` depends on NimBLE (added in plan 04). Enforce the layering rule by excluding `dd_*` from the native build.

```ini
[platformio]
default_envs = esp32dev

[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
build_flags =
    -Wall -Wextra
    -DAG_ENV_ESP32
; NimBLE dep added in plan 04
; lib_deps =
;     h2zero/NimBLE-Arduino @ ^1.4.0

[env:native]
platform = native
; In native env, skip dd_* libs (they include Arduino.h)
build_src_filter =
    +<*>
    -<../lib/dd_mpu6050/>
    -<../lib/dd_touch/>
    -<../lib/dd_ble_hid/>
    -<../lib/app_controller/>
build_flags =
    -std=gnu++17
    -Wall -Wextra
    -DAG_ENV_NATIVE
test_framework = unity
```

Notes:
- `test_framework = unity` applies to both envs; on-target tests live under `test/` too (plans 02–04 add them).
- Arduino framework is pulled in for `esp32dev`; `native` uses host libc.
- We intentionally exclude `app_controller` from native (it calls FreeRTOS). Native env tests only `srv_*`.

### 2. `.gitignore`

```
.pio/
.vscode/.browse.c_cpp.db*
.vscode/c_cpp_properties.json
.vscode/launch.json
.vscode/ipch
```

### 3. `src/main.cpp` (stdio-only bootstrap)

```cpp
#include <stdio.h>
#include "app_controller.h"

extern "C" void setup(void) {
    printf("[boot] AirGlove starting\n");
    if (app_controller_start() != AG_OK) {
        printf("[boot] app_controller_start failed\n");
    }
}

extern "C" void loop(void) {
    // App runs in FreeRTOS tasks; nothing to do here.
}
```

Discipline: **no** `Arduino.h`, `Wire.h`, or `BLEDevice.h` in this file. The `setup`/`loop` symbols are enough for the Arduino core to call into our code.

### 4. `lib/app_config/include/ag_types.h`

```c
#ifndef AG_TYPES_H
#define AG_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef int ag_result_t;
#define AG_OK          0
#define AG_ERR_IO     -1
#define AG_ERR_INIT   -2
#define AG_ERR_ARG    -3
#define AG_ERR_STATE  -4

// One 6-axis IMU sample in physical units.
typedef struct {
    float ax, ay, az;   // m/s^2
    float gx, gy, gz;   // rad/s
    uint64_t t_us;      // esp_timer_get_time() on target, monotonic otherwise
} imu_sample_t;

typedef enum {
    TOUCH_PAD_THUMB = 0,
    TOUCH_PAD_INDEX,
    TOUCH_PAD_MIDDLE,
    TOUCH_PAD_RING,
    TOUCH_PAD_COUNT
} touch_pad_id_t;

typedef struct {
    uint16_t raw[TOUCH_PAD_COUNT];
    uint8_t  touched_mask;   // bit N = TOUCH_PAD_<name> is currently below threshold
    uint64_t t_us;
} touch_sample_t;

// HID mouse report, wire-compatible with the BLE HID descriptor in dd_ble_hid.
typedef struct {
    int8_t  dx;
    int8_t  dy;
    uint8_t buttons;   // bit 0 = L, bit 1 = R, bit 2 = M
    int8_t  wheel;
} hid_mouse_report_t;

#endif
```

### 5. `lib/app_config/include/ag_pins.h`

```c
#ifndef AG_PINS_H
#define AG_PINS_H

// I2C — MPU6050
#define AG_PIN_I2C_SDA   21
#define AG_PIN_I2C_SCL   22

// Capacitive touch channels (GPIOs in parens)
#define AG_TOUCH_THUMB   0   // T0 / GPIO4  (common return)
#define AG_TOUCH_INDEX   2   // T2 / GPIO2
#define AG_TOUCH_MIDDLE  3   // T3 / GPIO15
#define AG_TOUCH_RING    4   // T4 / GPIO13

#endif
```

### 6. Driver stub headers (dd_*)

Reproduce the signatures from E02/E03/E04/E05 exactly. Example for `dd_mpu6050.h`:

```c
#ifndef DD_MPU6050_H
#define DD_MPU6050_H
#include "ag_types.h"
#ifdef __cplusplus
extern "C" {
#endif

ag_result_t dd_mpu6050_init(void);
ag_result_t dd_mpu6050_read(imu_sample_t *out);

#ifdef __cplusplus
}
#endif
#endif
```

Same pattern for `dd_touch.h` and `dd_ble_hid.h` (signatures live in E02).

### 7. Driver stub sources

Implementations return `AG_ERR_INIT` so callers see a deterministic failure until the real driver plan lands. Example:

```cpp
// lib/dd_mpu6050/src/dd_mpu6050.cpp
#include "dd_mpu6050.h"

extern "C" ag_result_t dd_mpu6050_init(void)      { return AG_ERR_INIT; }
extern "C" ag_result_t dd_mpu6050_read(imu_sample_t *out) {
    (void)out;
    return AG_ERR_STATE;
}
```

### 8. Service stub headers (srv_*)

Reproduce signatures from E06/E07/E08 epics verbatim. Bodies return `AG_OK` or a harmless identity value; tests will replace these.

### 9. `app_controller` stub

```cpp
// lib/app_controller/include/app_controller.h
#ifndef APP_CONTROLLER_H
#define APP_CONTROLLER_H
#include "ag_types.h"
#ifdef __cplusplus
extern "C" {
#endif
ag_result_t app_controller_start(void);
#ifdef __cplusplus
}
#endif
#endif

// lib/app_controller/src/app_controller.cpp
#include <stdio.h>
#include "app_controller.h"

extern "C" ag_result_t app_controller_start(void) {
    printf("[app_controller] stub\n");
    return AG_OK;
}
```

### 10. `library.json` per lib

Every lib uses the same shape. Example for `lib/srv_fusion/library.json`:

```json
{
  "name": "srv_fusion",
  "version": "0.0.1",
  "build": {
    "srcDir": "src",
    "includeDir": "include"
  }
}
```

For `dd_*` libs, add a hint that they require the Arduino framework by declaring a dependency in `platformio.ini` only (the library.json stays framework-neutral).

### 11. Unity dummy tests

Each `test/test_srv_*/test_main.cpp` contains:

```cpp
#include <unity.h>

void setUp(void) {}
void tearDown(void) {}

static void test_placeholder(void) {
    TEST_ASSERT_TRUE(true);
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_placeholder);
    return UNITY_END();
}
```

These exist so `pio test -e native` exits 0 until real tests are added by plans 05/06/07.

## Public header (target signatures)

All signatures are sourced from `.claude/memory/epics/E02-hw-abstraction-layer.md`, `E03`, `E04`, `E05`, `E06`, `E07`, `E08`, `E09`. **Do not invent new ones here.** Any mismatch between this plan and an epic → the epic wins; update the plan.

## Internal design notes

- **Why `extern "C"` on every public header?** To keep the C ABI stable so C++ (app layer) and pure-C (potential ports) can both consume the lib. Saves bikeshedding later.
- **Why separate `include/` and `src/` per lib?** PlatformIO treats `<lib>/include/` as the public-header path exported to consumers and `<lib>/src/` as private source. This is the one piece of structural layering the build system can enforce for free — we use it.
- **Why native-env filters exclude `dd_*`?** `dd_*` includes `Arduino.h` which does not exist on native. Excluding them in `build_src_filter` keeps the layering rule machine-checkable: the day a `srv_*` or `app_*` lib sneaks in an Arduino include, `pio run -e native` breaks.
- **Why stubs return errors?** So early callers get a deterministic `AG_ERR_INIT` instead of silently pretending everything works. Avoids "worked on my desk" false positives.

## Verification

After all files are in place:

```bash
cd air-glove
pio run -e esp32dev         # must compile (stub program)
pio run -e native           # must compile (services + test harness only)
pio test -e native          # must exit 0 with the three dummy tests passing
```

Manual traceability check:

- Every lib listed in `docs/srs/architecture.md` §3 has a folder in `lib/`.
- Every signature in the E02 epic is present verbatim in the corresponding header.
- `grep -r "Arduino.h" air-glove/lib/srv_*` returns nothing.
- `grep -r "Arduino.h" air-glove/lib/app_*` returns nothing.
- `grep "Arduino.h\|Wire.h\|BLEDevice.h" air-glove/src/main.cpp` returns nothing.

## Rollback / risk

- **Risk:** NimBLE-Arduino library will later conflict with the default ESP32 Arduino BT stack, causing linker errors. → Mitigation: declare NimBLE only in plan 04's `lib_deps` block, and keep Bluedroid-based includes out of the tree.
- **Risk:** `build_src_filter` syntax is finicky on Windows PlatformIO — if the `env:native` build fails with "no source found", verify the filter with `pio run -e native -v`.
- **Rollback:** delete `air-glove/` and reapply this plan; the directory is a clean slate, no external state to revert.

## References

- `.claude/memory/epics/E01-project-foundation.md`, `E02-hw-abstraction-layer.md`
- `docs/srs/architecture.md` §3 (component map), §4 (runtime model)
- `docs/srs/hardware.md` §2 (pinout)
- `docs/srs/decisions.md` ADR-005 (layering), ADR-004 (NimBLE)
- PlatformIO docs: <https://docs.platformio.org/en/latest/projectconf/index.html>
- Unity test framework: <https://docs.platformio.org/en/latest/advanced/unit-testing/frameworks/unity.html>
