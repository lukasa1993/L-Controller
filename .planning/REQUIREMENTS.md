# Requirements: LNH Nordic

**Defined:** 2026-03-08
**Core Value:** The device must reliably and safely execute configured local control actions—even through Wi-Fi disruption or subsystem faults—without unnecessary operator intervention.

## v1 Requirements

Requirements for the initial release. Each maps to roadmap phases.

### Platform Foundation

- [x] **PLAT-01**: Firmware is organized into separate modules for bootstrap, networking, recovery, HTTP/auth, actions, scheduling, persistence, OTA, and relay control
- [x] **PLAT-02**: `main.c` acts only as startup orchestration and does not contain subsystem business logic
- [x] **PLAT-03**: Shared subsystem interfaces use explicit data structures and avoid hidden global state

### Networking & Recovery

- [x] **NET-01**: Device can connect at boot using predefined build-time Wi-Fi credentials
- [ ] **NET-02**: Device can automatically recover from transient Wi-Fi disconnects without a full reboot
- [x] **NET-03**: Device exposes current connectivity state and last network failure state to the local admin panel
- [ ] **REC-01**: Device supervises critical execution paths with watchdog logic
- [ ] **REC-02**: Device triggers a full restart only after confirmed unrecoverable subsystem failure or watchdog starvation

### Authentication & Panel

- [ ] **AUTH-01**: Operator can log in with a single local username/password
- [ ] **AUTH-02**: Unauthenticated users cannot access control, configuration, scheduling, or update endpoints
- [ ] **AUTH-03**: Authenticated operator can log out and browser session state behaves predictably across page navigation
- [ ] **PANEL-01**: Device serves a local HTTP admin panel composed of authored HTML and JavaScript assets
- [ ] **PANEL-02**: Panel uses Tailwind Play CDN styling without generating UI markup in C
- [ ] **PANEL-03**: Panel shows current device, Wi-Fi, relay, scheduler, and update status

### Configuration & Persistence

- [ ] **CFG-01**: Device persists local authentication configuration across reboots
- [ ] **CFG-02**: Device persists relay and action configuration across reboots
- [ ] **CFG-03**: Device persists schedules across reboots
- [ ] **CFG-04**: Device validates persisted configuration on boot and falls back safely if data is corrupt or incompatible

### Actions & Relay Control

- [ ] **ACT-01**: Device supports a generic action definition model that can be extended beyond relay control later
- [ ] **ACT-02**: Manual panel commands and scheduled jobs execute through the same action engine path
- [ ] **RELAY-01**: Authenticated operator can activate the first relay from the panel
- [ ] **RELAY-02**: Authenticated operator can deactivate the first relay from the panel
- [ ] **RELAY-03**: Relay returns to a defined safe state on boot and fault recovery

### Scheduling

- [ ] **SCHED-01**: Operator can create cron-style schedules for relay actions
- [ ] **SCHED-02**: Operator can enable, disable, edit, and delete saved schedules
- [ ] **SCHED-03**: Scheduled relay actions execute locally without requiring external services
- [ ] **SCHED-04**: Device defines deterministic behavior for reboot, missed jobs, and time resynchronization

### Updates

- [ ] **OTA-01**: Authenticated operator can upload a firmware update locally through the panel or local API
- [ ] **OTA-02**: Device can be configured to pull an update from a remote endpoint
- [ ] **OTA-03**: Device stages firmware updates using a bootloader-backed process with rollback on failed boot
- [ ] **OTA-04**: Device marks a new firmware image permanent only after a healthy post-update boot

## v2 Requirements

### Observability

- **OBS-01**: Operator can view historical event and audit logs from the panel
- **OBS-02**: Device can export structured operational diagnostics for support workflows

### Hardware Expansion

- **RELAY-04**: Device can manage multiple relay channels
- **RELAY-05**: Device can assign schedules and actions to specific relay channels independently

### Integrations

- **INTG-01**: Device can execute non-relay actions through future SDK or integration adapters
- **INTG-02**: Operator can configure integration credentials and settings from the panel

### Access Model

- **AUTH-04**: Device supports more than one operator account
- **AUTH-05**: Device supports role-based permissions

## Out of Scope

| Feature | Reason |
|---------|--------|
| Public internet exposure of the admin panel | v1 is intentionally local-LAN-only over HTTP |
| Runtime Wi-Fi onboarding UI | Wi-Fi credentials remain build-time configured for the first release |
| Embedded scripting engine for arbitrary user logic | Too much safety and validation complexity for the first release |
| Multi-user account administration | Single-user admin flow is sufficient for v1 |
| Third-party SDK action implementations | Framework first; actual integrations come after the core stack is proven |

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| PLAT-01 | Phase 1 | Complete |
| PLAT-02 | Phase 1 | Complete |
| PLAT-03 | Phase 1 | Complete |
| NET-01 | Phase 2 | Complete |
| NET-02 | Phase 2 | Pending |
| NET-03 | Phase 2 | Complete |
| REC-01 | Phase 3 | Pending |
| REC-02 | Phase 3 | Pending |
| CFG-01 | Phase 4 | Pending |
| CFG-02 | Phase 4 | Pending |
| CFG-03 | Phase 4 | Pending |
| CFG-04 | Phase 4 | Pending |
| AUTH-01 | Phase 5 | Pending |
| AUTH-02 | Phase 5 | Pending |
| AUTH-03 | Phase 5 | Pending |
| PANEL-01 | Phase 5 | Pending |
| PANEL-02 | Phase 5 | Pending |
| PANEL-03 | Phase 5 | Pending |
| ACT-01 | Phase 6 | Pending |
| ACT-02 | Phase 6 | Pending |
| RELAY-01 | Phase 6 | Pending |
| RELAY-02 | Phase 6 | Pending |
| RELAY-03 | Phase 6 | Pending |
| SCHED-01 | Phase 7 | Pending |
| SCHED-02 | Phase 7 | Pending |
| SCHED-03 | Phase 7 | Pending |
| SCHED-04 | Phase 7 | Pending |
| OTA-01 | Phase 8 | Pending |
| OTA-02 | Phase 8 | Pending |
| OTA-03 | Phase 8 | Pending |
| OTA-04 | Phase 8 | Pending |

**Coverage:**
- v1 requirements: 31 total
- Mapped to phases: 31
- Unmapped: 0 ✓

---
*Requirements defined: 2026-03-08*
*Last updated: 2026-03-08 after roadmap creation*
