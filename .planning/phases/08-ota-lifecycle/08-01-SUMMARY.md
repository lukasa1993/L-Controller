---
phase: 08-ota-lifecycle
plan: "01"
subsystem: infra
tags: [mcuboot, sysbuild, ota, zephyr, partition-manager, nrf5340]
requires:
  - phase: 03-recovery-watchdog
    provides: boot health and recovery breadcrumbs that later OTA confirm/rollback work will reuse
  - phase: 04-persistent-configuration
    provides: typed NVS persistence ownership used for durable OTA metadata
  - phase: 05-local-control-panel
    provides: future authenticated update surfaces that will consume OTA service truth
provides:
  - sysbuild MCUboot firmware layout with an external secondary slot and merged or signed artifacts
  - ota_service boot ownership with current-image version truth and typed durable OTA metadata
  - one shared flash_img and MCUboot stage/apply path that rejects same-version and downgrade images
affects: [08-02, 08-03, panel, persistence, build]
tech-stack:
  added: [mcuboot, sysbuild, flash_img]
  patterns:
    - board overlay selects external flash for Partition Manager while app/sysbuild/mcuboot carries MCUboot child-image config
    - current firmware version comes from MCUboot headers while staged metadata and attempt bookkeeping live in typed persistence
    - all future OTA entrypoints must call ota_service staging and apply helpers instead of touching slot state directly
key-files:
  created:
    - app/src/ota/ota.c
    - app/sysbuild/mcuboot/prj.conf
    - app/sysbuild/mcuboot/boards/nrf7002dk_nrf5340_cpuapp.conf
    - app/sysbuild/mcuboot/boards/nrf7002dk_nrf5340_cpuapp.overlay
    - .planning/phases/08-ota-lifecycle/deferred-items.md
  modified:
    - app/pm_static.yml
    - app/sysbuild.conf
    - app/boards/nrf7002dk_nrf5340_cpuapp.overlay
    - app/src/app/bootstrap.c
    - app/src/persistence/persistence.c
    - scripts/build.sh
    - scripts/flash.sh
key-decisions:
  - "The nRF7002DK OTA foundation uses MCUboot sysbuild with mcuboot_secondary on MX25R64 external flash so the app keeps nearly the full internal slot."
  - "OTA metadata stays in a dedicated typed persistence section keyed by PERSISTENCE_NVS_ID_OTA without disturbing auth, relay, or schedule sections."
  - "Local upload and remote pull must share one ota_service pipeline that writes slot-1, reads MCUboot headers, rejects same-version and downgrade images, and requests only a test boot."
patterns-established:
  - "Board-owned OTA topology: the board overlay owns relay0 and nordic,pm-ext-flash while the MCUboot child image owns its own board-specific SPI NOR config."
  - "Typed OTA persistence: staged version, last-attempt result, rollback placeholders, and remote policy defaults flow through persistence_store_save_ota/load_ota only."
  - "Shared OTA runtime: begin_staging, write_chunk, finish_staging, and request_apply are the only supported transitions from candidate bytes to staged/apply-requested state."
requirements-completed: [OTA-03, OTA-04]
duration: 22 min
completed: 2026-03-10
---

# Phase 8 Plan 01: OTA Foundation Summary

**MCUboot sysbuild OTA foundation with external secondary-slot staging, typed OTA persistence, and one shared flash_img or MCUboot apply path**

## Performance

- **Duration:** 22 min
- **Started:** 2026-03-10T08:51:30Z
- **Completed:** 2026-03-10T09:13:11Z
- **Tasks:** 3
- **Files modified:** 19

## Accomplishments
- Switched the repo from a single-image app build to an MCUboot-backed sysbuild that produces `merged.hex`, `dfu_application.zip`, and signed app artifacts for the nRF7002DK target.
- Added a real `ota_service` to boot plus a typed OTA persistence section that keeps staged metadata, last-attempt results, rollback placeholders, and remote policy defaults inside the existing NVS boundary.
- Implemented one shared OTA plumbing path for later upload and remote-pull callers: write the secondary slot through `flash_img`, read MCUboot headers, reject same-version and downgrade images, and request only a non-permanent MCUboot test boot.

## Task Commits

Each task was committed atomically:

1. **Task 1: Convert the repo to an MCUboot sysbuild with a stable dual-slot partition map** - `638526f` (`feat`)
2. **Task 2: Add the OTA service boundary, config surface, and typed durable OTA state** - `072ab53` (`feat`)
3. **Task 3: Implement the shared stage-write, version-compare, and apply-request plumbing** - `7f32e74` (`feat`)

## Files Created/Modified

- `app/pm_static.yml` - Defines MCUboot, pad, primary app image, settings storage, and an external MX25R64 secondary slot.
- `app/sysbuild.conf` - Enables MCUboot sysbuild for the repo build entrypoint.
- `app/boards/nrf7002dk_nrf5340_cpuapp.overlay` - Combines the board-owned `relay0` alias with `nordic,pm-ext-flash` selection for OTA.
- `app/sysbuild/mcuboot/prj.conf` - Gives the MCUboot child image a valid local config root with sample-style bootloader settings.
- `app/sysbuild/mcuboot/boards/nrf7002dk_nrf5340_cpuapp.conf` - Enables SPI NOR support in the MCUboot child image for the external secondary slot.
- `app/src/ota/ota.h` - Declares the OTA runtime status plus shared staging and apply APIs.
- `app/src/ota/ota.c` - Implements slot writes, header reads, version comparison, and MCUboot apply requests.
- `app/src/persistence/persistence_types.h` - Adds the typed OTA persistence schema and save request types.
- `app/src/persistence/persistence.c` - Loads and saves the new OTA NVS section through the existing persistence boundary.
- `app/src/app/bootstrap.c` - Initializes OTA ownership during boot and logs current or staged OTA truth.
- `scripts/build.sh` - Builds with `--sysbuild` and reports the merged and signed artifact paths.
- `scripts/flash.sh` - Flashes the MCUboot sysbuild image set through `merged.hex`.
- `.planning/phases/08-ota-lifecycle/deferred-items.md` - Records pre-existing out-of-scope warnings surfaced during execution.

## Decisions Made

- Used external MX25R64 flash for `mcuboot_secondary` on the nRF7002DK instead of shrinking the primary app slot aggressively inside internal flash.
- Kept OTA durability inside a new typed `PERSISTENCE_NVS_ID_OTA` section rather than introducing ad hoc flash or raw NVS ownership outside the persistence subsystem.
- Made `ota_service` the only approved owner of staging and apply transitions so later local-upload and GitHub remote-pull work cannot diverge into separate slot-management paths.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Added the MCUboot child-image config root required by sysbuild**
- **Found during:** Task 1 (Convert the repo to an MCUboot sysbuild with a stable dual-slot partition map)
- **Issue:** Sysbuild failed while configuring the `mcuboot` child image because `app/sysbuild/mcuboot` did not contain its own `prj.conf`.
- **Fix:** Added `app/sysbuild/mcuboot/prj.conf` plus board-specific MCUboot overlay and SPI NOR config so the child image could build against the external flash secondary slot.
- **Files modified:** `app/sysbuild/mcuboot/prj.conf`, `app/sysbuild/mcuboot/boards/nrf7002dk_nrf5340_cpuapp.overlay`, `app/sysbuild/mcuboot/boards/nrf7002dk_nrf5340_cpuapp.conf`
- **Verification:** `./scripts/build.sh`
- **Committed in:** `638526f`

**2. [Rule 3 - Blocking] Restored the lost Phase 6 `relay0` board alias after pristine sysbuild exposed it**
- **Found during:** Task 1 (Convert the repo to an MCUboot sysbuild with a stable dual-slot partition map)
- **Issue:** A pristine sysbuild rebuild failed in `relay.c` because the current repo overlay no longer defined the `relay0` alias required by the existing relay service.
- **Fix:** Recovered the previously committed `gpio-leds`-based `relay0` binding from repo history and merged it into the updated nRF7002DK board overlay alongside `nordic,pm-ext-flash`.
- **Files modified:** `app/boards/nrf7002dk_nrf5340_cpuapp.overlay`
- **Verification:** `./scripts/build.sh`
- **Committed in:** `638526f`

---

**Total deviations:** 2 auto-fixed (2 blocking)
**Impact on plan:** Both fixes were required to make the planned MCUboot sysbuild shape buildable on a pristine checkout. No scope creep beyond the board and child-image config needed for correctness.

## Issues Encountered

- Sysbuild configuration initially stopped at the MCUboot child-image boundary until the local `app/sysbuild/mcuboot` config root was created.
- The first pristine rebuild surfaced a missing `relay0` board alias from earlier phase work; restoring that binding was necessary before OTA work could validate on a clean build.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Phase `08-02` can reuse `ota_service_begin_staging()`, `ota_service_write_chunk()`, `ota_service_finish_staging()`, and `ota_service_request_apply()` instead of touching slot state directly from HTTP handlers.
- Phase `08-03` can extend the already-persisted OTA remote policy defaults and last-attempt bookkeeping for GitHub pull plus confirm or rollback handling.
- Production release work still needs a project-owned MCUboot signing key; the current sysbuild uses Nordic's default debug key, which is acceptable for development only.

## Self-Check

PASSED

---
*Phase: 08-ota-lifecycle*
*Completed: 2026-03-10*
