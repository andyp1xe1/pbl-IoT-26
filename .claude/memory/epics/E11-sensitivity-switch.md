# E11 — Sensitivity Switch  [BACKLOG]

- **Status:** Backlog
- **Phase:** II
- **Realises:** FR-007

## Goal

Let the user cycle between ≥ 2 sensitivity presets at runtime without re-flashing firmware. Candidate mechanisms: hardware toggle switch, a dedicated long-press chord, or the ring-pad combined with a modifier.

## Scope (bullets — not yet broken down)

- Decide between hardware toggle (adds GPIO + switch) vs. chord-only (no HW change).
- Define presets (e.g., `LOW`, `NORMAL`, `HIGH`) as `motion_config_t` variants stored as constants in `app_config`.
- Extend `app_controller` with a "profile index" and a way to cycle it.
- Persist last-used preset in NVS so it survives reboots.
- Add host-visible feedback: briefly emit a tiny cursor jitter pattern to confirm profile change (since there's no on-board display).

## Promotion criteria

Gate on E09 acceptance, and ideally on some early user-testing feedback on Phase I feel so preset magnitudes are grounded in data, not guesses.

## Progress log

- 2026-04-21: Epic stub created.
