---
phase: 04-persistent-configuration
plan: "01"
subsystem: infra
tags: [zephyr, nvs, flash-map, partition-manager, persistence, configuration]
requires:
  - phase: 01-foundation-refactor
    provides: app_boot-owned configuration loading and subsystem boundaries
  - phase: 03-recovery-watchdog
    provides: current app_context boot sequencing reused by persistence initialization
provides:
  - reserved `settings_storage` flash partition for durable configuration data
  - versioned auth, action, relay, and schedule persistence contracts
  - boot-owned persistence mount and snapshot load boundary in `app_boot()`
affects: [04-02, 04-03, panel, actions, relay, scheduler]
tech-stack:
  added: [NVS, flash_map, Partition Manager static partition]
  patterns: [boot-owned persistence seam, section-scoped fallback handling, safe clean-device defaults]
key-files:
  created:
    - app/pm_static.yml
    - app/src/persistence/persistence.c
    - app/src/persistence/persistence_types.h
    - .planning/phases/04-persistent-configuration/04-01-SUMMARY.md
    - .planning/phases/04-persistent-configuration/deferred-items.md
  modified:
    - app/CMakeLists.txt
    - app/Kconfig
    - app/prj.conf
    - app/src/app/app_config.h
    - app/src/app/app_context.h
    - app/src/app/bootstrap.c
    - app/src/persistence/persistence.h
key-decisions:
  - "Use a dedicated NVS-backed `settings_storage` partition instead of leaking flash details into future services."
  - "Keep `app_context` as the single owner of both the mounted persistence backend and the resolved persisted snapshot."
  - "Reseed only the auth section from build-time credentials while leaving other sections on safe empty defaults or section-local resets."
patterns-established:
  - "Boot-owned persistence seam: `app_boot()` loads the resolved snapshot before downstream services start."
  - "Section-scoped recovery: invalid or incompatible data resets only the affected section."
  - "Operator-meaningful load reporting: each section records load state plus auth reseed information."
requirements-completed: [CFG-01, CFG-02, CFG-03, CFG-04]
duration: 13 min
completed: 2026-03-09
---

# Phase 4 Plan 1: Persistence Boundary Summary

**NVS-backed persistent configuration boundary with versioned auth/action/relay/schedule records and boot-owned snapshot loading**

## Performance

- **Duration:** 13 min (elapsed from the initial `04-01` task commit to resumed completion)
- **Started:** 2026-03-09T08:08:26.000Z
- **Completed:** 2026-03-09T08:21:51Z
- **Tasks:** 3
- **Files modified:** 10

## Accomplishments

- Reserved a dedicated `settings_storage` partition, enabled flash-backed persistence primitives, and exposed the phase-specific layout and relay reboot configuration surface.
- Defined explicit versioned persisted records for auth, actions, relay state, and schedules, with per-section load outcomes that distinguish loaded, empty-default, invalid-reset, incompatible-reset, and auth reseed cases.
- Wired persistence ownership into `app_context` and `app_boot()` so the store mounts and the resolved snapshot loads before downstream subsystems start.

## Task Commits

Each task was committed atomically:

1. **Task 1: Add the persistence backend surface and build integration** - `5c22ac8` (`feat`)
2. **Task 2: Define versioned section types and load/fallback reporting** - `b580e95` (`feat`)
3. **Task 3: Wire boot ownership for persistence without pulling future features forward** - `611334b` (`feat`)

## Files Created/Modified

- `app/pm_static.yml` - Reserves the `settings_storage` partition used by the NVS-backed persistence store.
- `app/Kconfig` - Declares persistence layout and clean-device relay reboot-policy knobs.
- `app/prj.conf` - Enables the flash, flash-map, and NVS primitives needed by the backend.
- `app/src/app/app_config.h` - Exposes persistence layout and reboot-policy config to boot-owned code.
- `app/src/app/app_context.h` - Stores the mounted persistence backend and resolved persisted snapshot by value.
- `app/src/app/bootstrap.c` - Initializes the persistence store, loads the snapshot, and logs section outcomes before downstream startup.
- `app/src/persistence/persistence.h` - Defines the public store contract and section-scoped load/save APIs.
- `app/src/persistence/persistence.c` - Implements NVS mount/load/save behavior, defaults, validation, and section fallback handling.
- `app/src/persistence/persistence_types.h` - Defines versioned persisted section types and load-report structures.

## Decisions Made

- Chose Zephyr NVS over a higher-level settings API so the persistence backend can own exact section IDs, validation, and reset behavior centrally.
- Kept auth seeding inside the persistence boundary and exposed only typed load outcomes to the rest of the app.
- Logged per-section load states during boot so clean defaults and fallback resets are visible without inspecting raw storage.

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

- `./scripts/build.sh` completed successfully but surfaced pre-existing MBEDTLS Kconfig warnings unrelated to Phase 4 persistence work. Logged in `.planning/phases/04-persistent-configuration/deferred-items.md`.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- `04-02` can build repository-style auth, relay/action, and schedule save flows on top of the typed snapshot and section APIs created here.
- `04-03` can now exercise corruption, incompatibility, and reboot-persistence scenarios against a real boot-mounted store.
- No new blocker was introduced in `04-01`; later manual hardware verification remains a separate future gate and was not fabricated here.

## Self-Check

**PASSED**

- FOUND: .planning/phases/04-persistent-configuration/04-01-SUMMARY.md
- FOUND: 5c22ac8
- FOUND: b580e95
- FOUND: 611334b
