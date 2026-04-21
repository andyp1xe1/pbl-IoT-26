# E10 — Scroll & Clutch  [BACKLOG]

- **Status:** Backlog
- **Phase:** II
- **Realises:** FR-005, FR-006

## Goal

Add two usability features on top of the Phase I MVP: a **scroll gesture** (designated finger chord emits vertical wheel events while cursor motion is inhibited) and a **clutch** (temporary suspension of cursor motion for hand repositioning, via a distinct chord or held gesture).

## Scope (bullets — not yet broken down)

- Designate the scroll chord (candidate: thumb↔ring) and clutch trigger (candidate: thumb↔pinky or a held index contact).
- Extend `srv_input` chord semantics to expose "chord active" state edges.
- Extend `srv_motion` to multiplex: when scroll-chord active, route tilt to `wheel` field instead of `dx/dy`.
- Extend `srv_motion` clutch gate (already present from E07) to be driven by the clutch chord.
- Update `app_controller` FSM with `SCROLL` and `CLUTCHED` sub-states.
- Update `docs/srs/testing-strategy.md` with TC-FR05-* and TC-FR06-*.

## Promotion criteria

Move this epic from Backlog to Active after E09 acceptance is met and a short bench demo validates Phase I feel. At that point, fill in Public Interface / Acceptance / Dependencies / Plan file.

## Progress log

- 2026-04-21: Epic stub created; not yet planned in detail.
