# Project Research Summary

**Project:** LNH Nordic
**Domain:** Embedded local-control firmware with operator-configured relay actions
**Researched:** 2026-03-11
**Confidence:** HIGH

## Executive Summary

This milestone should not introduce a new platform. The correct move is to keep the existing Nordic/Zephyr panel, persistence, scheduler, and dispatcher stack, then replace the last fixed relay assumptions with one operator-managed action catalog. The key architectural boundary is that the browser should configure named actions, while the firmware remains the only owner of which GPIO-backed outputs are valid and safe to drive.

The biggest risk is migration: the shipped system still seeds built-in `relay0.on` / `relay0.off` actions and the panel/scheduler contracts are hard-coded around those two choices. The milestone needs an explicit migration layer, then a shared public action catalog so the Actions page and Schedules page consume the same configured action list.

## Key Findings

### Recommended Stack

Keep the current NCS v3.2.1 stack and existing Zephyr HTTP/persistence modules. Official Zephyr docs continue to reinforce a devicetree-backed GPIO model and flash-backed settings/NVS patterns, which fits an embedded allowlisted output-binding approach much better than arbitrary raw GPIO entry from the browser.

**Core technologies:**
- Nordic Connect SDK `v3.2.1`: keep the existing firmware and board baseline
- Zephyr GPIO/devicetree model: use firmware-known output bindings for safe hardware ownership
- Existing typed persistence layer: extend schema and migration instead of replacing storage

### Expected Features

**Must have (table stakes):**
- Relay action CRUD with operator-safe name/ID and approved output binding
- Configured-action visibility on the Actions page and schedule action picker
- Real gating so only valid configured actions are executable or schedulable
- Safe migration for old built-in action IDs and schedule references

**Should have (competitive):**
- Clear reasons when an action is unavailable
- Schedule-impact protection on action edit/delete
- Generic action schema shape that can support future non-relay actions

**Defer (v2+):**
- Arbitrary pin discovery from the browser
- Non-relay action implementations
- Bulk/template workflows for many outputs

### Architecture Approach

Grow `actions/` into the catalog owner, add exact action-management routes in `panel_http`, publish catalog snapshots through `panel_status`, keep schedules storing only canonical `action_id`, and resolve configured actions to firmware-known output bindings before dispatch. This keeps manual control, scheduling, and persistence aligned while preserving the mission-critical runtime boundaries already in the repo.

**Major components:**
1. Action catalog service - CRUD, public keys/labels, migration, and dispatcher lookup
2. Output binding registry - approved GPIO-backed outputs hidden behind firmware-owned bindings
3. Panel/API layer - action management plus schedule selection from the shared catalog
4. Scheduler integration - schedule validation/reload when the action catalog changes

### Critical Pitfalls

1. **Unsafe raw GPIO configuration** - store logical output bindings, not arbitrary pin text
2. **Split action catalogs** - keep one source of truth for Actions and Schedules
3. **Stale schedule references** - revalidate/reload schedules after every action mutation
4. **Legacy built-ins lingering** - define migration behavior explicitly and remove unconditional seeding
5. **Relay-only bypass paths** - do not leave the panel on `/api/relay/desired-state` once the catalog exists

## Implications for Roadmap

Based on research, suggested phase structure:

### Phase 9: Action Data Model and Migration
**Rationale:** The current shipped state still hard-codes built-in relay action IDs, so schema and migration must move first.
**Delivers:** Configurable action records, approved output-binding model, boot/save validation, migration path from legacy built-ins.
**Addresses:** Relay action CRUD foundation, visibility gating, legacy compatibility.
**Avoids:** Unsafe GPIO storage and hidden built-in fallback behavior.

### Phase 10: Action Management UI and Shared API Surface
**Rationale:** Once the catalog exists safely, the panel and scheduler can both consume it.
**Delivers:** Actions-page management, schedule picker from shared catalog, delete/edit impact handling, generic action execution path.
**Uses:** Existing HTTP server, panel asset pipeline, scheduler reload path.
**Implements:** Shared public action catalog across Actions and Schedules.

### Phase Ordering Rationale

- Migration and validation have to land before UI CRUD, otherwise the panel will only paper over old built-in assumptions.
- The scheduler should keep depending on `action_id`, so the catalog must be stable before the UI starts mutating it.
- This order removes the relay-only bypass path without putting hardware safety at risk.

### Research Flags

Phases likely needing deeper research during planning:
- **Phase 9:** exact migration behavior for existing persisted `relay0.on` / `relay0.off` references
- **Phase 10:** operator experience for deleting or disabling actions that existing schedules still depend on

Phases with standard patterns:
- **Phase 10:** authenticated exact-route JSON CRUD and shared snapshot rendering follow established repo conventions

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | Repo already runs on the recommended stack and official docs support the hardware/storage constraints |
| Features | HIGH | Directly grounded in the current shipped gaps and user-requested operator flow |
| Architecture | HIGH | Existing module boundaries map cleanly to the needed changes |
| Pitfalls | HIGH | Risks are visible in the current relay-only assumptions and migration boundary |

**Overall confidence:** HIGH

### Gaps to Address

- Legacy migration policy: decide whether old built-in actions are translated, replaced, or invalidated at boot
- Binding model detail: decide whether operator configuration chooses from approved output IDs only or also configures output labels separately

## Sources

### Primary
- [Zephyr GPIO docs](https://docs.zephyrproject.org/latest/hardware/peripherals/gpio.html)
- [Zephyr devicetree how-tos](https://docs.zephyrproject.org/latest/build/dts/howtos.html)
- [Zephyr settings docs](https://docs.zephyrproject.org/latest/services/storage/settings/index.html)
- [Zephyr NVS docs](https://docs.zephyrproject.org/latest/services/storage/nvs/nvs.html)

### Local codebase
- `app/src/actions/actions.c`
- `app/src/panel/panel_http.c`
- `app/src/panel/panel_status.c`
- `app/src/panel/assets/main.js`
- `app/src/persistence/persistence.c`
- `app/src/persistence/persistence_types.h`
- `app/src/scheduler/scheduler.c`

---
*Research completed: 2026-03-11*
*Ready for roadmap: yes*
