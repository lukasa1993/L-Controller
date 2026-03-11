# Roadmap: LNH Nordic

## Overview

Milestone `v1.0` completed phases `1` through `8` and established the current production-shaped baseline: modular firmware, robust Wi-Fi and recovery, persistent configuration, local authenticated panel, action dispatcher, scheduling, and OTA lifecycle. Milestone `v1.1` continues numbering with a deliberately compact roadmap that replaces built-in relay actions with configured relay actions that are named, bound to approved outputs, visible under Actions, and selectable from Schedules only after valid configuration.

## Phase Ledger

- [x] **Phase 1: Foundation Refactor** - Split the monolithic firmware into explicit subsystem boundaries and keep the existing boot path working
- [x] **Phase 2: Wi-Fi Supervision** - Introduce a dedicated network supervisor with robust reconnect and visibility into connectivity state
- [x] **Phase 3: Recovery & Watchdog** - Add conservative fault supervision and reset escalation for true stuck states
- [x] **Phase 4: Persistent Configuration** - Establish durable, validated storage for auth, actions, relay state rules, and schedules
- [x] **Phase 5: Local Control Panel** - Ship the authenticated local HTTP panel with authored HTML/JS assets and operator status views
- [x] **Phase 6: Action Engine & Relay Control** - Implement the generic action path and the first safe relay on/off behavior
- [x] **Phase 7: Scheduling** - Add cron-style local scheduling for relay actions with deterministic reboot/time behavior
- [x] **Phase 8: OTA Lifecycle** - Deliver bootloader-backed local upload and remote-pull firmware updates with rollback discipline
- [ ] **Phase 9: Configured Action Model and Panel Management** - Replace fixed relay action assumptions with a validated configured relay-action catalog and operator CRUD surface
- [ ] **Phase 10: Shared Execution and Schedule Integration** - Make configured relay actions usable everywhere and verify that manual and scheduled execution share the same truth

## Phase Details

### Phase 9: Configured Action Model and Panel Management
**Goal:** Establish the configured relay-action data model, safe output binding rules, migration policy, and operator management surface that replace built-in relay action assumptions.
**Depends on:** Phase 4, Phase 5, Phase 6, Phase 7
**Requirements:** [ACT-03, ACT-04, ACT-05, BIND-01, BIND-02, MIGR-01, MIGR-03]
**Success Criteria** (what must be TRUE):
1. Device loads configured relay actions from persisted records or a safe migration path and no longer depends on unconditional built-in relay seeding as the primary model.
2. Authenticated operator can create, view, and edit configured relay actions with operator-safe IDs, names, enabled state, usability status, and output summary in the panel.
3. Device accepts only approved output bindings for configured relay actions and rejects unsafe bindings before they become usable.
4. Configured relay action records survive reboot, legacy built-in action references are handled explicitly, and boot validation never falls back to hidden built-in execution.
**Plans:** Not planned yet

### Phase 10: Shared Execution and Schedule Integration
**Goal:** Make configured relay actions executable from the Actions page, selectable from Schedules, and enforced consistently across manual and scheduled runtime paths.
**Depends on:** Phase 9
**Requirements:** [ACT-06, ACT-07, ACT-08, BIND-03, BIND-04, SCHED-05, SCHED-06, SCHED-07, MIGR-02]
**Success Criteria** (what must be TRUE):
1. Actions page manual control executes configured relay actions instead of fixed built-in relay commands and blocks invalid or disabled actions.
2. Actions page and schedule forms expose the same shared configured action catalog, and invalid or disabled actions are not selectable for schedules.
3. Action delete or mutation flows explicitly preserve schedule integrity so saved schedules never silently reference missing or unusable actions, including migrated legacy schedule references.
4. Manual and scheduled execution both resolve through the same canonical action ID path, with end-to-end verification recorded.
**Plans:** Not planned yet

## Progress

**Execution Order:**
Phases execute in numeric order: `1 -> 2 -> 3 -> 4 -> 5 -> 6 -> 7 -> 8 -> 9 -> 10`

| Phase | Status | Completed |
|-------|--------|-----------|
| 1. Foundation Refactor | Complete | 2026-03-08 |
| 2. Wi-Fi Supervision | Complete | 2026-03-08 |
| 3. Recovery & Watchdog | Complete | 2026-03-09 |
| 4. Persistent Configuration | Complete | 2026-03-09 |
| 5. Local Control Panel | Complete | 2026-03-10 |
| 6. Action Engine & Relay Control | Complete | 2026-03-10 |
| 7. Scheduling | Complete | 2026-03-10 |
| 8. OTA Lifecycle | Complete | 2026-03-10 |
| 9. Configured Action Model and Panel Management | Pending | - |
| 10. Shared Execution and Schedule Integration | Pending | - |

## Current Milestone Summary

- Milestone: `v1.1 Action Configuration Flow`
- New phases: `2`
- New requirements mapped: `16`
- Phase cap honored: `2 phases maximum`

## Next Step

**Phase 9: Configured Action Model and Panel Management** is the next phase to discuss and plan.
