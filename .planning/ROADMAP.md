# Roadmap: LNH Nordic

## Overview

LNH Nordic moves from a single-file Wi-Fi bring-up firmware into a mission-critical embedded control product in eight tightly scoped phases. The roadmap establishes structure first, then hardens connectivity and recovery, then adds the local authenticated control plane, then ships the first real relay action, followed by scheduling and OTA lifecycle support.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [x] **Phase 1: Foundation Refactor** - Split the monolithic firmware into explicit subsystem boundaries and keep the existing boot path working
- [x] **Phase 2: Wi-Fi Supervision** - Introduce a dedicated network supervisor with robust reconnect and visibility into connectivity state
- [x] **Phase 3: Recovery & Watchdog** - Add conservative fault supervision and reset escalation for true stuck states
- [x] **Phase 4: Persistent Configuration** - Establish durable, validated storage for auth, actions, relay state rules, and schedules (completed 2026-03-09)
- [ ] **Phase 5: Local Control Panel** - Ship the authenticated local HTTP panel with authored HTML/JS assets and operator status views
- [x] **Phase 6: Action Engine & Relay Control** - Implement the generic action path and the first real relay on/off behavior (completed 2026-03-10)
- [x] **Phase 7: Scheduling** - Add cron-style local scheduling for relay actions with deterministic reboot/time behavior (completed 2026-03-10)
- [ ] **Phase 8: OTA Lifecycle** - Deliver bootloader-backed local upload and remote-pull firmware updates with rollback discipline

## Phase Details

### Phase 1: Foundation Refactor
**Goal**: Replace the single `main.c` application structure with modular subsystem boundaries while preserving the current build and baseline bring-up behavior.
**Depends on**: Nothing (first phase)
**Requirements**: [PLAT-01, PLAT-02, PLAT-03]
**Success Criteria** (what must be TRUE):
  1. Firmware builds with subsystem-specific source/header layout and `main.c` reduced to composition/bootstrap responsibilities.
  2. Shared application interfaces use explicit typed state/config structures instead of hidden cross-module globals.
  3. Existing baseline startup still reaches the current ready state through the new bootstrap structure.
**Plans**: 3/3 plans executed

Plans:
- [x] 01-01: Define module layout, common interfaces, and shared app context boundaries
- [x] 01-02: Extract bootstrap and existing runtime flow out of `main.c` into subsystem entry points
- [x] 01-03: Add basic validation scaffolding so the refactor does not regress the current bring-up path

### Phase 2: Wi-Fi Supervision
**Goal**: Move Wi-Fi lifecycle ownership into a dedicated supervisor that can reconnect robustly and publish precise connectivity state.
**Depends on**: Phase 1
**Requirements**: [NET-01, NET-02, NET-03]
**Success Criteria** (what must be TRUE):
  1. Device boots using predefined Wi-Fi credentials through a supervisor-owned connection flow.
  2. Transient disconnects trigger bounded reconnect behavior without forcing a whole-device reboot.
  3. The system exposes clear connectivity state and last network failure information for downstream services and the future panel.
**Plans**: 3/3 plans executed

Plans:
- [x] 02-01: Implement the network supervisor state machine and event handling boundaries
- [x] 02-02: Add reconnect policy, backoff, and reachability-state reporting
- [x] 02-03: Validate the supervisor against common Wi-Fi failure modes and regressions

### Phase 3: Recovery & Watchdog
**Goal**: Add layered watchdog and recovery behavior that resets the device only for confirmed unrecoverable conditions.
**Depends on**: Phase 2
**Requirements**: [REC-01, REC-02]
**Success Criteria** (what must be TRUE):
  1. Critical execution paths are supervised by watchdog logic rather than a single generic feed loop.
  2. Minor or transient faults do not trigger device reset loops.
  3. Confirmed stuck states escalate through a defined recovery path to full restart when necessary.
**Plans**: 2/2 plans executed

Plans:
- [x] 03-01: Implement task-level supervision and escalation policy boundaries
- [x] 03-02: Validate stuck-state detection, escalation thresholds, and reset semantics

### Phase 4: Persistent Configuration
**Goal**: Create a validated persistent configuration layer for auth, actions, relay rules, and schedules.
**Depends on**: Phase 1
**Requirements**: [CFG-01, CFG-02, CFG-03, CFG-04]
**Success Criteria** (what must be TRUE):
  1. Device loads persisted auth, relay/action, and schedule configuration correctly after reboot.
  2. Configuration changes survive reboot and unexpected power loss within the supported write model.
  3. Corrupt or incompatible stored data is detected and handled safely.
  4. Persistent data access is centralized behind typed configuration APIs.
**Plans**: 3/3 plans complete

Plans:
- [x] 04-01: Define storage schema, versioning rules, and persistence boundaries
- [x] 04-02: Implement typed repositories for auth, action/relay, and schedule configuration
- [x] 04-03: Add corruption handling, migration hooks, and persistence validation tests

### Phase 5: Local Control Panel
**Goal**: Deliver the local authenticated HTTP panel using authored frontend assets and protected operator endpoints.
**Depends on**: Phase 2, Phase 3, Phase 4
**Requirements**: [AUTH-01, AUTH-02, AUTH-03, PANEL-01, PANEL-02, PANEL-03]
**Success Criteria** (what must be TRUE):
  1. Unauthenticated requests cannot access protected control, schedule, configuration, or update operations.
  2. A single operator can log in, use the local panel, and log out through a predictable browser session flow.
  3. The panel is served from authored HTML/JS assets styled via Tailwind Play CDN, not from C-generated markup.
  4. The panel exposes current device, Wi-Fi, relay, scheduler, and update status.
**Plans**: 3/3 plans executed

Plans:
- [x] 05-01: Build the HTTP service shell and static asset delivery pipeline
- [x] 05-02: Implement single-user auth, sessions, and protected API routing
- [ ] 05-03: Ship the initial operator dashboard and status views

### Phase 6: Action Engine & Relay Control
**Goal**: Establish the generic action execution path and ship the first safe relay on/off operation.
**Depends on**: Phase 4, Phase 5
**Requirements**: [ACT-01, ACT-02, RELAY-01, RELAY-02, RELAY-03]
**Success Criteria** (what must be TRUE):
  1. Authenticated operator can activate and deactivate the first relay through the panel/API.
  2. Manual commands and future scheduled jobs execute through the same action engine path.
  3. Relay control is isolated behind a driver abstraction with defined safe boot and recovery state.
**Plans**: 3/3 plans executed

Plans:
- [x] 06-01: Implement the relay driver abstraction and safe-state policy
- [x] 06-02: Implement the generic action model and execution pipeline
- [x] 06-03: Wire panel/API relay commands through the action engine and validate end-to-end behavior *(approved browser/curl/device verification recorded on 2026-03-10)*

### Phase 7: Scheduling
**Goal**: Add persistent cron-style scheduling for relay actions with deterministic time and reboot semantics.
**Depends on**: Phase 4, Phase 6
**Requirements**: [SCHED-01, SCHED-02, SCHED-03, SCHED-04]
**Success Criteria** (what must be TRUE):
  1. Operator can create, edit, enable, disable, and delete relay schedules from the local panel/API.
  2. Scheduled jobs persist across reboot and execute locally without external services.
  3. Scheduler behavior is clearly defined for reboot, missed jobs, and time resynchronization.
  4. Scheduled executions use the same action path as manual commands.
**Plans**: 3/3 plans executed

Plans:
- [x] 07-01: Define the time model, schedule schema, and missed-job semantics
- [x] 07-02: Implement the scheduler engine and persistent job storage
- [x] 07-03: Add schedule management endpoints/UI and validate execution behavior *(approved browser/curl/device verification recorded on 2026-03-10)*

### Phase 8: OTA Lifecycle
**Goal**: Introduce safe firmware update lifecycle support for local upload and remote pull using a rollback-capable boot flow.
**Depends on**: Phase 3, Phase 4, Phase 5
**Requirements**: [OTA-01, OTA-02, OTA-03, OTA-04]
**Success Criteria** (what must be TRUE):
  1. Authenticated operator can upload a firmware image locally to the staged update path.
  2. Device can be configured to fetch an update from a remote endpoint without bypassing the same image safety rules.
  3. Firmware updates use a bootloader-backed swap/rollback process rather than replacing the active image in place.
  4. A new firmware image is only marked permanent after a healthy post-update boot.
**Plans**: 3 plans

Plans:
- [x] 08-01: Enable MCUboot/sysbuild, partitioning, and staged-image plumbing
- [ ] 08-02: Implement authenticated local upload and staged image validation flow
- [ ] 08-03: Implement remote-pull orchestration plus confirm/rollback handling

## Progress

**Execution Order:**
Phases execute in numeric order: 1 → 2 → 3 → 4 → 5 → 6 → 7 → 8

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Foundation Refactor | 3/3 | Complete | 2026-03-08 |
| 2. Wi-Fi Supervision | 3/3 | Complete | 2026-03-08 |
| 3. Recovery & Watchdog | 2/2 | Complete | 2026-03-09 |
| 4. Persistent Configuration | 3/3 | Complete   | 2026-03-09 |
| 5. Local Control Panel | 2/3 | In Progress | - |
| 6. Action Engine & Relay Control | 3/3 | Complete | 2026-03-10 |
| 7. Scheduling | 3/3 | Complete | 2026-03-10 |
| 8. OTA Lifecycle | 2/3 | In Progress|  |
