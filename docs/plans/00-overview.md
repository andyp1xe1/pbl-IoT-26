# Plan 00 — Overview & Critical Path

This folder holds the **detailed, ordered implementation plans** for the Air Glove Phase I MVP. Each plan realises one epic in `.claude/memory/epics/`. The docs here are the "how"; the epics are the "what + why".

## Reading order

Read plans in the numeric order of their prefix. Each plan states its preconditions at the top; don't start a plan whose preconditions are not green.

| Plan | Epic | Title | Depends on |
|------|------|-------|------------|
| 01 | E01, E02 | Scaffolding — PlatformIO, lib skeletons, interface contracts | — |
| 02 | E03 | `dd_mpu6050` — I²C IMU driver | 01 |
| 03 | E04 | `dd_touch` — capacitive touch driver | 01 |
| 04 | E05 | `dd_ble_hid` — NimBLE HID mouse transport | 01 |
| 05 | E06 | `srv_fusion` — Madgwick filter | 01, 02 (for I/O types) |
| 06 | E07 | `srv_motion` — quat → dx/dy mapping | 05 |
| 07 | E08 | `srv_input` — debounce + edge detection | 01, 03 (for types) |
| 08 | E09 | `app_controller` — tasks, queues, FSM | 02, 03, 04, 05, 06, 07 |
| 09 | E09 | Integration & bring-up — HIL checklist | 08 |

## Critical path

```
01 ──► 02 ──► 05 ──► 06 ──┐
  └──► 03 ──► 07 ────────┤
  └──► 04 ───────────────┴──► 08 ──► 09
```

Plans 02–07 are largely parallelisable once plan 01 is done (the scaffold provides interface contracts that let every later lib compile in isolation).

## Per-plan contract

Every plan below must contain:

- **Epic** — link to the owning `.claude/memory/epics/E0X-*.md`.
- **Goal** — 1–2 sentences.
- **Preconditions** — other plans or artefacts required.
- **Files to create** — relative paths with one-line roles.
- **Step-by-step** — actionable steps, register names / library calls where specific.
- **Public header** — exact signatures lifted from the owning epic.
- **Internal design notes** — algorithm, state, memory, thread-safety.
- **Verification** — unit/on-target tests + PlatformIO command to run them + manual probes.
- **Rollback / risk** — smallest revert if the plan fails; known risk + early warning.
- **References** — architecture.md sections, decisions ADRs, epic IDs, external docs.

## When to update a plan

Plans are authoritative for an in-flight epic. If a plan becomes stale during implementation (a library version bumped, a register address corrected, a signature tweaked), **fix the plan first**, then update the code. Out-of-date plans are worse than no plans.

## How plans, epics, and requirements relate

```
requirements.md  ──owns── FR / NFR IDs
      │
      │ (realised by)
      ▼
.claude/memory/epics/Exx  ──owns── scope, acceptance, progress
      │
      │ (executed via)
      ▼
docs/plans/NN-*.md        ──owns── step-by-step, signatures, verification
      │
      │ (produces)
      ▼
air-glove/lib/<name>/     ──owns── actual code, unit tests
```

Change requests flow top-down: a new/changed FR → touch the owning epic → touch the owning plan → touch the code.

## Open meta-questions

- **Plan 09 HIL acceptance** requires at least one Windows, one Linux, and one macOS host available. If a host is unavailable, that row in the TC-NFR-HID-001 matrix is marked "Not Verified" — never "Pass".
- **Latency measurement** requires either a logic analyser or a mouse-event-timestamping script on the host. Plan 09 provides the script; the hardware method is a nice-to-have.
