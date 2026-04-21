# Air Glove — Requirements Catalogue

Last updated: 2026-04-21
Source narrative: `report/chapters/requirements.tex`, `report/chapters/ch3_proposed_solution.tex`
This file is the **distilled machine-tractable view**. For rationale, user stories, and diagrams, read the LaTeX report.

---

## Functional requirements

| ID | Phase | Summary | Acceptance hint | Realising epic(s) |
|----|-------|---------|-----------------|-------------------|
| FR-001 | I | Device initialises and announces itself as a standard HID mouse on power-on. | Device boots within 3 s; advertises "AirGlove" over BLE; status indicator shows "ready". | E01, E05, E09 |
| FR-002 | I | Bluetooth pairing with auto-reconnection to the last known host. | First pairing via OS dialog; after reboot, reconnects to the last host within 5 s without re-pairing. | E05, E09 |
| FR-003 | I | Mid-air cursor XY motion control driven by IMU orientation. | Tilting glove produces cursor motion in the corresponding axis; stable (no drift) when held still. | E03, E06, E07, E09 |
| FR-004 | I | Click event generation through finger contact. | Thumb↔index = left click; thumb↔middle = right click; single contact → single click event on host. | E04, E08, E09 |
| FR-005 | II (backlog) | Scroll interaction mode triggered by dedicated finger chord. | Thumb↔ring chord (or designated gesture) emits vertical wheel events while cursor motion is inhibited. | E10 |
| FR-006 | II (backlog) | Clutch / hand-repositioning suspends cursor motion on demand. | User-triggered clutch pauses dx/dy output until released; click still works. | E10 |
| FR-007 | II (backlog) | Adjustable sensitivity profiles without firmware editing. | Hardware toggle or chord cycles between ≥ 2 sensitivity presets at runtime. | E11 |
| FR-008 | II (backlog) | User calibration workflow for neutral orientation and zero-bias. | Single user action triggers calibration; persisted across reboots. | E12 |
| FR-009 | III (backlog) | Fault-safe runtime behaviour with sensor anomaly detection. | IMU hang or I²C timeout is detected and recovered without user reset. | E13 |
| FR-010 | III (backlog) | Standard Bluetooth HID compatibility across OS platforms without drivers. | Same firmware works on Windows 10+, Linux 5.4+, macOS 12+, Android/iOS mobile — no driver install. | E05, E13 |

---

## Non-functional requirements

| ID | Category | Target | Measurement | Realising epic(s) |
|----|----------|--------|-------------|-------------------|
| NFR-LAT-001 | Latency | IMU-to-host cursor event ≤ 100 ms p95 | Host-side timestamping (see `docs/srs/testing-strategy.md` TC-NFR-LAT-001). | E06, E07, E05, E09 |
| NFR-STAB-001 | Stability | Cursor drift at rest < 2 px/s median over 10 s | TC-FR03 acceptance probe. | E06, E07 |
| NFR-PWR-001 | Power | ≥ 4 h continuous session on a 500 mAh LiPo (target; TBD) | Bench test; tracked by E14 (backlog). | E14 (backlog) |
| NFR-HID-001 | Interoperability | Works on Windows 10+, Linux 5.4+, macOS 12+ with no driver install | TC-NFR-HID-001. | E05 |
| NFR-ERG-001 | Ergonomics | Total glove mass ≤ 80 g; no exposed conductors on fingertips | Bench weigh + visual inspection. | E01 (hardware section), E04 |
| NFR-MOD-001 | Modularity | `app_*` and `srv_*` libs MUST NOT include Arduino/ESP-IDF headers; only `dd_*` may. | Build-time enforcement: `srv_*` and `app_*` compile cleanly in `env:native`. | E01, E02 |

---

## Requirement → Epic traceability matrix

| Epic | Realises |
|------|----------|
| E01 Project Foundation | NFR-MOD-001, NFR-ERG-001 (setup only) |
| E02 HW Abstraction Layer | NFR-MOD-001 |
| E03 IMU Driver (dd_mpu6050) | FR-003 |
| E04 Touch Driver (dd_touch) | FR-004, NFR-ERG-001 |
| E05 BLE HID Driver (dd_ble_hid) | FR-001, FR-002, FR-010, NFR-HID-001, NFR-LAT-001 |
| E06 Sensor Fusion Service (srv_fusion) | FR-003, NFR-STAB-001, NFR-LAT-001 |
| E07 Motion Mapping Service (srv_motion) | FR-003, NFR-STAB-001, NFR-LAT-001 |
| E08 Input Service (srv_input) | FR-004 |
| E09 Application Controller (app_controller) | FR-001, FR-002, FR-003, FR-004, NFR-LAT-001 |
| E10 Scroll & Clutch (backlog) | FR-005, FR-006 |
| E11 Sensitivity Switch (backlog) | FR-007 |
| E12 Calibration (backlog) | FR-008 |
| E13 Fault Safety (backlog) | FR-009, FR-010 |
| E14 Power Management (backlog) | NFR-PWR-001 |

---

## Out of scope (for this project)

- TinyML / on-device gesture classification (noted as future path in research report).
- 2.4 GHz USB dongle variant (research mentioned it as fallback; not built here).
- Multi-user profile storage beyond a single calibration set.
- Companion mobile app.

## Open requirement questions

- **NFR-PWR-001** target (4 h) is provisional — needs validation against measured MVP draw (tracked by E14).
- **FR-005 scroll gesture** — chord choice (thumb↔ring vs. dedicated switch) to be finalised when E10 is activated.
- **NFR-STAB-001** drift threshold (2 px/s) is an estimate — to be tightened after first real measurement in E06.
