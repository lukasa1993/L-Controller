---
phase: 06-action-engine-relay-control
plan: "01"
subsystem: firmware
tags: [zephyr, gpio, devicetree, relay, recovery]

requires:
  - phase: 03-recovery-watchdog
    provides: retained recovery reset breadcrumbs used to force a safe relay-off startup path
  - phase: 04-persistent-configuration
    provides: persisted relay desired-state and reboot-policy inputs
  - phase: 05-local-control-panel
    provides: shared boot and panel status surfaces that will consume live relay runtime state
provides:
  - GPIO-backed relay runtime for the first board-owned relay output
  - Boot-time startup policy that distinguishes ordinary reboot policy from recovery-reset safety overrides
  - Read-only relay status with actual state, remembered desired state, source, and safety note
affects: [06-02 action dispatcher, 06-03 relay panel api, relay validation]

tech-stack:
  added: [Zephyr GPIO devicetree binding]
  patterns: [board-owned relay alias, relay service runtime ownership, boot-time safety policy evaluation]

key-files:
  created: [app/boards/nrf7002dk_nrf5340_cpuapp.overlay, app/src/relay/relay.c, .planning/phases/06-action-engine-relay-control/deferred-items.md]
  modified: [app/CMakeLists.txt, app/prj.conf, app/src/app/app_context.h, app/src/app/bootstrap.c, app/src/relay/relay.h]

key-decisions:
  - "The board mapping for relay 0 lives behind a `relay0` devicetree alias so HTTP and future action code stay hardware-agnostic."
  - "The relay runtime preserves remembered desired state even when boot or recovery policy forces the applied state off."
  - "Boot initializes recovery breadcrumbs before the relay service so startup policy can distinguish ordinary reboots from recovery resets."

patterns-established:
  - "`relay_service` owns applied relay hardware state and exposes read-only runtime status for later consumers."
  - "Startup safety policy is resolved during boot before panel HTTP startup, using persisted relay policy plus recovery breadcrumbs."

requirements-completed: [RELAY-03]

duration: 3 min
completed: 2026-03-10
---

# Phase 6 Plan 1: Relay Runtime Foundations Summary

**GPIO-backed relay startup runtime with board-owned `relay0` binding, configurable normal reboot policy, and recovery-forced safe-off boot behavior**

## Performance

- **Duration:** 3 min
- **Started:** 2026-03-10T03:38:22Z
- **Completed:** 2026-03-10T03:41:26Z
- **Tasks:** 3
- **Files modified:** 7

## Accomplishments

- Added the first board-owned relay output binding and Zephyr GPIO build wiring for `nrf7002dk/nrf5340/cpuapp`.
- Implemented a real `relay_service` runtime that tracks actual state, remembered desired state, source, and safety note while applying persisted reboot policy.
- Wired relay initialization into boot after persistence and recovery breadcrumbs are available, before the panel HTTP shell starts.

## Task Commits

Each task was committed atomically:

1. **Task 1: Add relay build wiring and board-owned first-relay binding** - `1e15188` (feat)
2. **Task 2: Implement relay runtime state and explicit startup policy** - `faa896c` (feat)
3. **Task 3: Compose relay ownership into boot before panel startup** - `fd826d0` (feat)

## Files Created/Modified

- `app/CMakeLists.txt` - Builds the relay runtime source with the rest of the application.
- `app/prj.conf` - Enables the Zephyr GPIO subsystem required by the relay service.
- `app/boards/nrf7002dk_nrf5340_cpuapp.overlay` - Defines the board-owned `relay0` alias and polarity for the first relay output.
- `app/src/relay/relay.h` - Exposes relay runtime status, source metadata, and the read-only status getter.
- `app/src/relay/relay.c` - Owns GPIO-backed relay startup policy evaluation and live runtime state.
- `app/src/app/app_context.h` - Stores `relay_service` inside the shared application context.
- `app/src/app/bootstrap.c` - Initializes recovery breadcrumbs and relay runtime before the panel shell starts.
- `.planning/phases/06-action-engine-relay-control/deferred-items.md` - Captures unrelated pre-existing warnings discovered during execution.

## Decisions Made

- Kept the physical relay mapping in the board overlay via `relay0` so future HTTP and action layers never need raw GPIO knowledge.
- Treated recovery-reset boot as a stricter safety override than ordinary reboot, forcing the applied relay state off without erasing remembered desired state.
- Logged resolved relay runtime state during boot so later panel and manual validation work can confirm actual-versus-desired behavior quickly.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking Issue] Added a recognized devicetree binding for the `relay0` node**
- **Found during:** Task 1 (Add relay build wiring and board-owned first-relay binding)
- **Issue:** The initial custom relay overlay node exposed `gpios`, but Zephyr did not generate the property accessors needed by `GPIO_DT_SPEC_GET`, so the first build failed in `app/src/relay/relay.c`.
- **Fix:** Marked `relay_outputs` as `compatible = "gpio-leds"` and added a `label` so the board-owned `relay0` alias remained intact while the GPIO phandle accessors were generated correctly.
- **Files modified:** `app/boards/nrf7002dk_nrf5340_cpuapp.overlay`
- **Verification:** `./scripts/build.sh`
- **Committed in:** `1e15188` (part of task commit)

---

**Total deviations:** 1 auto-fixed (1 Rule 3 build blocker)
**Impact on plan:** Required to satisfy the build gate for the new relay runtime. No scope creep beyond making the planned board binding valid for Zephyr GPIO accessors.

## Issues Encountered

- Pre-existing MBEDTLS Kconfig warnings still appear during configuration.
- A pre-existing `unused variable 'wifi_config'` compiler warning remains in `app/src/network/network_supervisor.c`.
- Both out-of-scope warnings were logged to `deferred-items.md` and did not block the plan’s build verification.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- The action dispatcher can now target `relay_service` as the sole owner of applied relay hardware state.
- Future panel/API work can consume live relay status fields for actual state, remembered desired state, source, and safety-note rendering.
- Recovery-aware startup semantics are now in place for manual validation of RELAY-03 behavior on hardware.


## Self-Check: PASSED

FOUND: `.planning/phases/06-action-engine-relay-control/06-01-SUMMARY.md`
FOUND: `app/CMakeLists.txt`
FOUND: `app/prj.conf`
FOUND: `app/boards/nrf7002dk_nrf5340_cpuapp.overlay`
FOUND: `app/src/relay/relay.h`
FOUND: `app/src/relay/relay.c`
FOUND: `app/src/app/app_context.h`
FOUND: `app/src/app/bootstrap.c`
FOUND: `.planning/phases/06-action-engine-relay-control/deferred-items.md`
FOUND: `1e15188`
FOUND: `faa896c`
FOUND: `fd826d0`
