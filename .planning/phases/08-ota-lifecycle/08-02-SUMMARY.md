---
phase: 08-ota-lifecycle
plan: "02"
subsystem: ota
tags: [ota, mcuboot, zephyr-http, panel-ui, firmware-upload]
requires:
  - phase: 08-01
    provides: MCUboot sysbuild foundation, ota_service staging/apply plumbing, and typed OTA persistence
provides:
  - Authenticated `/api/update` routes with streaming local firmware upload, explicit apply, and staged-image clear semantics
  - Dedicated panel OTA workflow with staged version visibility, upload controls, pending-update warning, and apply confirmation
  - Truthful compact OTA summary in `/api/status` while richer OTA detail stays on `/api/update`
affects: [08-03, panel, ota, remote-update]
tech-stack:
  added: []
  patterns: [streaming binary upload through ota_service, exact authenticated OTA routes, compact aggregate status with dedicated detail route]
key-files:
  created: []
  modified: [app/prj.conf, app/src/ota/ota.h, app/src/ota/ota.c, app/src/panel/panel_http.c, app/src/panel/panel_status.c, app/src/panel/assets/index.html, app/src/panel/assets/main.js]
key-decisions:
  - "The panel exposes OTA detail on exact `/api/update/*` routes while `/api/status` stays compact and summary-oriented."
  - "Local firmware upload validates `Content-Type` and `Content-Length`, streams chunks into `ota_service`, and never buffers the full image in RAM."
  - "Explicit apply schedules a delayed reboot after `boot_request_upgrade(false)` so the operator receives a response before the browser session drops."
patterns-established:
  - "Pattern: register Zephyr header capture explicitly for every request header a streaming route must validate."
  - "Pattern: keep OTA status split between aggregate summary truth (`/api/status`) and a richer dedicated OTA route family (`/api/update`)."
requirements-completed: [OTA-01, OTA-03]
duration: 32 min
completed: 2026-03-10
---

# Phase 8 Plan 2: Local Upload and Staged Update Surface Summary

**Authenticated local OTA upload with streaming stage-first routes, explicit apply reboot semantics, and a dedicated panel workflow for staged firmware visibility**

## Performance

- **Duration:** 32 min
- **Started:** 2026-03-10T09:17:30Z
- **Completed:** 2026-03-10T09:49:03Z
- **Tasks:** 3
- **Files modified:** 7

## Accomplishments

- Added authenticated exact `/api/update`, `/api/update/upload`, `/api/update/apply`, and `/api/update/clear` routes that delegate into `ota_service`.
- Replaced the placeholder update card with a dedicated OTA surface that stages local firmware first and warns explicitly about reboot plus session loss before apply.
- Replaced the placeholder `/api/status.update` payload with compact live OTA truth covering current version, staged version, last result, rollback flags, and apply readiness.

## Task Commits

Each task was committed atomically:

1. **Task 1: Add dedicated authenticated OTA routes with binary-safe upload and explicit apply semantics** - `0bc659e` (feat)
2. **Task 2: Replace the placeholder update card with a dedicated update workflow** - `59ae400` (feat)
3. **Task 3: Publish truthful OTA summary status and staged-image readiness** - `d9554fe` (feat)

## Files Created/Modified

- `app/prj.conf` - Increased Zephyr captured-header capacity so the upload route can validate `Content-Type` and `Content-Length`.
- `app/src/ota/ota.h` - Added OTA service abort and clear entry points for panel upload and operator reset flows.
- `app/src/ota/ota.c` - Added stage abort and clear behavior, stage-failure cleanup, and flash-area close handling during finish and abort paths.
- `app/src/panel/panel_http.c` - Added the authenticated OTA route family, streaming upload handler, detailed OTA JSON response, and delayed reboot scheduling after explicit apply.
- `app/src/panel/panel_status.c` - Replaced the placeholder update summary with compact live OTA state for `/api/status`.
- `app/src/panel/assets/index.html` - Extended the visual shell to support the dedicated OTA workflow surface.
- `app/src/panel/assets/main.js` - Added `/api/update` refresh logic and the staged OTA UI flow for upload, clear, and apply.

## Decisions Made

- Kept `/api/status` compact by surfacing only summary OTA truth there and reserving richer detail plus mutations for the dedicated `/api/update` surface.
- Used a binary `application/octet-stream` upload contract backed by explicit captured request headers because the existing JSON-body buffer pattern is not safe for firmware images.
- Delayed the post-apply reboot from the HTTP layer so the operator receives an explicit response before the connection drops and RAM session state is cleared.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Enabled Zephyr header capture for `Content-Type` and `Content-Length`**
- **Found during:** Task 1 (Add dedicated authenticated OTA routes with binary-safe upload and explicit apply semantics)
- **Issue:** Zephyr only exposes request headers that are explicitly captured, so the upload route could not validate binary-safe OTA uploads with the existing `Cookie`-only capture setup.
- **Fix:** Registered `Content-Type` and `Content-Length` capture in the panel HTTP layer and increased `CONFIG_HTTP_SERVER_CAPTURE_HEADER_COUNT`.
- **Files modified:** `app/prj.conf`, `app/src/panel/panel_http.c`
- **Verification:** `rg -n "/api/update|upload|apply|clear|application/octet-stream|content-length|ota_service|same-version|downgrade" app/src/panel/panel_http.c app/src/ota/ota.h app/src/ota/ota.c`
- **Committed in:** `0bc659e`

**2. [Rule 1 - Bug] Closed OTA staging flash areas on finish and added explicit abort cleanup**
- **Found during:** Task 1 (Add dedicated authenticated OTA routes with binary-safe upload and explicit apply semantics)
- **Issue:** The 08-01 OTA stage-finalization path did not close the staging flash area after finishing, and there was no shared abort helper for failed or interrupted upload streams.
- **Fix:** Added `ota_service_abort_staging()`, `ota_service_clear_staged_image()`, and consistent flash-area close handling on finish and abort paths.
- **Files modified:** `app/src/ota/ota.h`, `app/src/ota/ota.c`
- **Verification:** `./scripts/build.sh`
- **Committed in:** `0bc659e`

---

**Total deviations:** 2 auto-fixed (1 blocking, 1 bug)
**Impact on plan:** Both deviations were required for correctness of the streaming upload contract. No scope creep beyond the planned OTA surface.

## Issues Encountered

- The execution shell reported a non-zero exit code on the full build command even when `./scripts/build.sh` printed `Build complete` and emitted all signed artifacts. The plan used the successful build output and generated artifacts as the verification signal.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- The local upload path, staged-image truth, and explicit apply boundary are now ready for 08-03 remote-pull orchestration.
- The remaining OTA work is the remote GitHub flow plus post-boot confirm and rollback handling.

## Self-Check: PASSED

- Verified summary file exists at `.planning/phases/08-ota-lifecycle/08-02-SUMMARY.md`
- Verified task commit hashes `0bc659e`, `59ae400`, and `d9554fe` exist in repository history

---
*Phase: 08-ota-lifecycle*
*Completed: 2026-03-10*
