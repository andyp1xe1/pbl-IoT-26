# Air Glove — Memory Index

This folder is the **single source of truth** for Air Glove project tracking.
It is checked into git and must stay in sync with code.

## Layout

```
.claude/memory/
├── README.md            ← you are here
├── requirements.md      ← FR/NFR catalogue with traceability
├── progress.md          ← one-line status per epic (dashboard)
└── epics/
    ├── E01-…-E09-…      ← Phase I MVP epics (actively planned)
    └── E10-…-E14-…      ← backlog epics (Phase II/III, skeleton only)
```

## How to use it

- **Requirement change** (new FR/NFR, acceptance tweak) → edit `requirements.md`, bump its `Last updated` line, touch the owning epic's Progress Log.
- **Code change inside an epic's scope** → append a dated line to that epic's *Progress Log* (`- 2026-04-21: started dd_mpu6050 skeleton`).
- **Epic status change** (Not Started → In Progress → Blocked → Done) → update the epic header AND the matching row in `progress.md`.
- **Backlog promotion** (E10+ moves from "Backlog" to "Active") → the epic owner rewrites it from skeleton to full template and unlocks it in `progress.md`.

## Where does detailed planning live?

Not here. Implementation plans (step-by-step, with signatures + verification) live in `docs/plans/`. Each active epic points to its own plan file via the `Owns:` header field.

## Where does the requirements narrative live?

The full LaTeX SRS under `report/` is the narrative/rationale document. `requirements.md` is the **machine-tractable distillation** — IDs, one-sentence intent, traceability — suitable for driving implementation and review.

## Related SRS artefacts (outside this folder)

- `docs/srs/architecture.md` — layered architecture + FreeRTOS task map.
- `docs/srs/hardware.md` — BOM, pinout, wiring.
- `docs/srs/decisions.md` — ADR log.
- `docs/srs/testing-strategy.md` — test layers + traceability.
- `docs/srs/glossary.md` — terms, IDs, task names.

## Conventions

- Epic IDs: `E01`…`E14`, two-digit zero-padded, never renumbered.
- FR IDs: `FR-001`…`FR-010` (match LaTeX report verbatim).
- NFR IDs: `NFR-<CATEGORY>-<seq>` (e.g., `NFR-LAT-001`).
- Test case IDs: `TC-<REQ-ID>-<seq>` (defined in `docs/srs/testing-strategy.md`).
