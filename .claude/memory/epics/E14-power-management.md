# E14 — Power Management  [BACKLOG]

- **Status:** Backlog
- **Phase:** III
- **Realises:** NFR-PWR-001

## Goal

Hit the battery-life target (≥ 4 h continuous session, 500 mAh LiPo). Requires idle-state reduction, wake-on-touch, and probably duty-cycling the BLE connection interval when no motion is detected.

## Scope (bullets — not yet broken down)

- Characterise actual draw per task using a current-sense breakout (target baseline before changes).
- ESP32 light-sleep between IMU samples when motion magnitude < threshold for > N seconds.
- Wake sources: touch pad (ESP32 supports `touch_pad_wakeup`).
- BLE connection-interval negotiation: request longer interval (e.g., 30 ms) when idle, tighter (7.5–15 ms) during active motion.
- Battery gauge: either pull a TP4056 `CHG` signal or add a simple ADC divider to read cell voltage; expose as low-battery heartbeat.
- Acceptance: 4 h continuous use from full charge on bench mix (50 % active / 50 % idle).

## Promotion criteria

After Phase II or when the NFR-PWR-001 measurement from E09 bring-up shows we are below target (likely — `docs/srs/hardware.md` §5 flags this).

## Progress log

- 2026-04-21: Epic stub created.
