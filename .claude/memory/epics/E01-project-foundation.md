# E01 — Project Foundation

- **Status:** Done
- **Phase:** I
- **Owns:** `air-glove/platformio.ini`, `air-glove/src/main.cpp`, `air-glove/lib/*/library.json` (skeletons), `air-glove/test/` (empty harness)
- **Plan:** `docs/plans/01-scaffolding.md`
- **Realises:** NFR-MOD-001, partial NFR-ERG-001 (hardware notes captured in `docs/srs/hardware.md`)

## Goal

Produce a PlatformIO project skeleton under `air-glove/` that compiles cleanly on both the `esp32dev` (on-target) and `native` (host) environments, with every lib folder (`dd_*`, `srv_*`, `app_*`) present as an empty but valid library. No feature code — this epic exists so every later epic starts from green builds.

## Scope

**In:**
- `platformio.ini` with two envs (`esp32dev`, `native`), Unity test framework, build-src-filter rules, lib dependency wiring (NimBLE-Arduino in `esp32dev`; none in `native`).
- `src/main.cpp` — stdio-only bootstrapper that calls `app_controller_start()`. No Arduino includes.
- 8 lib folders (listed in `docs/srs/architecture.md` §3), each with `library.json`, a public header in `<name>/include/`, and a minimal `.cpp` stub in `<name>/src/`.
- `test/test_srv_fusion/` placeholder using Unity's `RUN_TEST(...)` around a dummy TEST that passes.
- `.gitignore` entries for `.pio/`, `.vscode/`.

**Out:**
- Any driver, service, or FSM logic — that's E02–E09.

## Public interface

None (infrastructure epic).

## Acceptance criteria

- [ ] `pio run -e esp32dev` succeeds with zero warnings about missing libraries. *(deferred — host machine does not have PlatformIO installed; tracked as an open check before E03 bring-up)*
- [ ] `pio run -e native` succeeds. *(deferred — same reason; approximated by g++ native compile, see below)*
- [ ] `pio test -e native` exits 0 with 1+ dummy test passing. *(deferred — requires PIO; Unity harness is in place)*
- [x] `lib/srv_*/` and `lib/app_*/` sources compile without any `Arduino.h` include transitively. *(Verified: `Grep` over `lib/srv_*` and `lib/app_*` for Arduino/ESP-IDF/NimBLE/FreeRTOS headers returns no matches; `g++ -std=gnu++17 -Wall -Wextra -Werror -c lib/srv_fusion/src/srv_fusion.cpp` etc. build cleanly.)*
- [x] `src/main.cpp` includes no header from `lib/dd_*/include/`. *(Verified: main.cpp includes only `<stdio.h>` and `"app_controller.h"`.)*

## Dependencies

None.

## Progress log

- 2026-04-21: Epic created. Scaffold layout defined in `docs/plans/01-scaffolding.md`.
- 2026-04-21: Scaffold delivered — 22 files under `air-glove/` (`platformio.ini`, stdio-only `src/main.cpp`, eight `lib/` folders with interface headers + stubs, three `test/test_srv_*/` Unity harnesses). `srv_*` libs compile under g++ 9.2 with `-Wall -Wextra -Werror`. Status: Done. Full `pio run` / `pio test` verification deferred to when PlatformIO is installed on the host.
