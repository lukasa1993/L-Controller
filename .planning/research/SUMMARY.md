# Project Research Summary

**Project:** LNH Nordic
**Domain:** Configurable relay action management for an embedded local control panel
**Researched:** 2026-03-11
**Confidence:** HIGH

## Executive Summary

This milestone should not introduce a new platform. The repo already has the right base: Nordic/Zephyr firmware, typed NVS-backed persistence, authenticated panel routes, a shared dispatcher, and schedules. The missing layer is a real operator-managed action catalog that replaces built-in relay assumptions with named, validated, configurable relay actions.

The safest design is to keep hardware knowledge in firmware through a compile-time output registry and let operator-managed actions reference approved logical bindings. Research across the current codebase and official Zephyr GPIO, Settings, and NVS guidance points in the same direction: do not treat arbitrary raw GPIO strings as trusted runtime truth, do not split manual and scheduled action catalogs, and do not let migration leave hidden built-in relay paths alive.

The highest-risk areas are migration from `relay0.on` / `relay0.off`, schedule integrity when actions are edited or deleted, and flash wear from noisy persistence writes. The roadmap should therefore start with the binding model and migration rules before shipping panel CRUD.

## Key Findings

### Recommended Stack

The recommended stack is mostly the current stack. The milestone needs new schema, catalog, UI/API, and runtime integration work, not new infrastructure.

**Core technologies:**
- Nordic Connect SDK `v3.2.1`: keep the current firmware/platform baseline
- Zephyr GPIO plus devicetree device model: represent approved hardware outputs safely
- Existing typed persistence over NVS-backed storage: persist action config with validation and migration
- Existing panel HTTP plus JSON contracts: expose CRUD and schedule-choice surfaces
- Existing dispatcher plus scheduler: keep one execution path for manual and scheduled work

### Expected Features

The table-stakes flow is straightforward: create relay actions, validate the binding before use, show them under Actions, and make them selectable from Schedules through one shared catalog. Operators also need safe edit/delete behavior so schedules do not silently break.

**Must have (table stakes):**
- Action CRUD for configured relay actions
- Validation-before-use
- Shared catalog for Actions and Schedules
- Enable/disable state
- Safe delete/edit behavior for referenced actions
- Safe migration from built-in relay assumptions

**Should have (competitive):**
- Usage visibility for schedule references
- Action health/status badges
- Migration assistance for existing devices

**Defer (v2+):**
- Non-relay action types
- Multi-output expansion
- Bulk templates or grouped actions

### Architecture Approach

The new architecture boundary is an output registry plus an action catalog service. The output registry owns approved hardware bindings. The action catalog service owns operator-managed records, public labels/keys, migration, and referential integrity. Panel routes, scheduler choices, and execution all consume that shared truth.

**Major components:**
1. Output registry - maps logical output IDs to approved GPIO bindings
2. Action catalog service - owns configured relay actions and public metadata
3. Persistence/migration boundary - validates and commits action plus schedule changes safely
4. Panel CRUD/API surface - lets operators manage configured actions
5. Scheduler and dispatcher integration - use the same configured action catalog

### Critical Pitfalls

1. **Unsafe raw GPIO configuration** - avoid by using approved logical bindings instead of arbitrary pin entry
2. **Split manual and schedule catalogs** - avoid by feeding both pages from one persisted action catalog
3. **Stale schedule references** - avoid by validating or blocking destructive action edits/deletes
4. **Legacy built-ins lingering** - avoid by defining an explicit migration/removal plan for `relay0.on` / `relay0.off`
5. **Relay-only bypass routes surviving** - avoid by moving manual execution onto the configured action model

## Implications for Roadmap

Based on research, suggested phase structure:

### Phase 9: Output Binding Model and Migration
**Rationale:** safe hardware binding and schema migration must exist before operators can manage actions
**Delivers:** output registry, persistence schema changes, boot validation, and legacy relay-action migration policy
**Addresses:** validation-before-use, migration, hidden built-in removal
**Avoids:** unsafe raw GPIO config and half-migrated runtime behavior

### Phase 10: Action Catalog CRUD and Shared UI/API Projection
**Rationale:** once the data model is safe, the panel needs a real operator-managed catalog surface
**Delivers:** authenticated action CRUD routes, Actions page management UI, dynamic schedule action choices, and dependency-aware edit/delete rules
**Uses:** existing panel/auth stack and persistence boundary
**Implements:** one shared catalog for Actions and Schedules

### Phase 11: Runtime Integration and Verification
**Rationale:** configured actions are only complete once manual control, schedules, migration, and hardware execution all use the same truth
**Delivers:** dispatcher/runtime integration, schedule execution validation, and end-to-end browser/device verification
**Uses:** existing dispatcher, scheduler, relay runtime, and validation scripts

### Phase Ordering Rationale

- Binding and migration rules come first because they constrain every later UI/API decision.
- CRUD and shared projection come second because Actions and Schedules must evolve together.
- Runtime verification comes last because it depends on the final catalog, schedule, and migration contract.

### Research Flags

Phases likely needing deeper research during planning:
- **Phase 9:** exact migration behavior from built-in relay IDs to configured actions
- **Phase 11:** final executable-action model for schedule/manual control if one configured relay resource yields multiple operations

Phases with standard patterns:
- **Phase 10:** CRUD, protected routes, and snapshot projection follow established repo patterns

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | Grounded in official Zephyr docs and the current repo baseline |
| Features | HIGH | Directly constrained by the user request and the repo's current fixed relay assumptions |
| Architecture | HIGH | Integration points are visible in actions, panel, scheduler, and persistence modules |
| Pitfalls | HIGH | Risks are concrete and already visible in the current implementation |

**Overall confidence:** HIGH

### Gaps to Address

- Migration policy: seed one configured relay action automatically, or require explicit post-upgrade configuration
- Execution model: whether schedule/manual surfaces target derived `on/off` executable choices or a higher-level configured output plus desired-state field

## Sources

### Primary (HIGH confidence)
- [Zephyr GPIO docs](https://docs.zephyrproject.org/latest/hardware/peripherals/gpio.html)
- [Zephyr Devicetree how-tos](https://docs.zephyrproject.org/latest/build/dts/howtos.html)
- [Zephyr Settings docs](https://docs.zephyrproject.org/latest/services/storage/settings/index.html)
- [Zephyr NVS docs](https://docs.zephyrproject.org/latest/services/storage/nvs/nvs.html)
- Local repo analysis of `app/src/actions`, `app/src/panel`, `app/src/persistence`, and `app/src/scheduler`

### Secondary (MEDIUM confidence)
- Existing planning context in `.planning/PROJECT.md`, `.planning/REQUIREMENTS.md`, and `.planning/codebase/`

---
*Research completed: 2026-03-11*
*Ready for roadmap: yes*
