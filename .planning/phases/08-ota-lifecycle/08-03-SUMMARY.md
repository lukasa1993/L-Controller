---
phase: 08-ota-lifecycle
plan: "03"
subsystem: ota-lifecycle
tags: [ota, github, mcuboot, rollback, validation]
requires:
  - phase: 08-01
    provides: MCUboot sysbuild foundation, shared OTA staging/apply runtime, and typed OTA persistence
  - phase: 08-02
    provides: authenticated OTA routes, staged update UI, and truthful OTA status surfaces
provides:
  - GitHub Releases remote-update flow plus authenticated `Update now`
  - Post-`APP_READY` confirmation and rollback visibility across reboot
  - Build-first validation flow plus approved browser/curl/device Phase 8 sign-off
affects: [phase-complete, panel, ota, persistence, docs]
tech-stack:
  added: [http_client, downloader, tls_credentials]
  patterns:
    - GitHub release metadata fetched over HTTPS, asset download streamed through the same OTA staging path as local upload
    - MCUboot confirmation delayed until the new image survives the healthy post-boot stable window
    - phase validation remains build-first with blocking real-device OTA approval
key-files:
  created:
    - app/src/ota/ota_github_ca_certs.h
    - .planning/phases/08-ota-lifecycle/08-03-SUMMARY.md
  modified:
    - app/Kconfig
    - app/prj.conf
    - app/src/app/bootstrap.c
    - app/src/ota/ota.h
    - app/src/ota/ota.c
    - app/src/persistence/persistence_types.h
    - app/src/persistence/persistence.c
    - app/src/panel/panel_http.c
    - app/src/panel/panel_status.c
    - app/src/panel/assets/main.js
    - README.md
    - scripts/build.sh
    - scripts/common.sh
    - scripts/validate.sh
key-decisions:
  - "Remote OTA is fixed to the latest stable GitHub Release from lukasa1993/L-Controller and reuses the same OTA staging/apply service as local upload."
  - "A test image becomes permanent only after APP_READY plus the configured stable window, not at stage time or immediately after reboot."
  - "Phase 8 approval stays build-first and blocks on explicit browser/curl/device confirmation of upload, apply, confirm, rollback, GitHub update-now, and retry behavior."
patterns-established:
  - "Remote OTA metadata and asset downloads stay inside ota_service so panel handlers remain thin authenticated triggers."
  - "Rollback truth is durable through the typed OTA persistence section and surfaced back through both /api/status and /api/update."
  - "`./scripts/validate.sh` remains the canonical automated path while the hardware checklist records the irreversible OTA scenarios automation cannot prove."
requirements-completed: [OTA-02, OTA-03, OTA-04]
duration: 14 min (auto tasks) + approved human verification
completed: 2026-03-10
---

# Phase 8 Plan 03: Remote OTA, Confirmation, Rollback, and Validation Summary

**GitHub-backed remote OTA, delayed MCUboot confirmation after healthy boot, rollback visibility, and approved real-device Phase 8 verification**

## Performance

- **Duration:** 14 min (auto tasks) + approved human verification
- **Started:** 2026-03-10T14:22:44+04:00
- **Checkpointed:** 2026-03-10T14:38:01+04:00
- **Completed:** 2026-03-10T20:06:43+04:00
- **Tasks completed:** 4 of 4
- **Files modified:** 14

## Accomplishments

- Added a GitHub Releases remote-update flow that checks the latest stable `lukasa1993/L-Controller` release, selects a signed OTA artifact, and streams it through the same OTA staging/apply service used by local upload.
- Added an authenticated `Update now` trigger plus remote OTA status fields so the panel can show when the device is checking, downloading, or applying a remote update.
- Added delayed post-`APP_READY` confirmation so a booted test image is only marked permanent after the healthy stable window, while failed confirmation paths surface as explicit rollback status on the next boot.
- Updated the Phase 8 README and validation scripts so the repo now documents the full build-first plus blocking hardware/browser OTA lifecycle.
- Recorded approved real-device verification for upload, same-version and downgrade rejection, apply, confirmation, rollback visibility, GitHub update-now, daily retry, and panel truthfulness.

## Task Commits

Each implementation task was committed atomically before final closeout:

1. **Task 1: Add the GitHub Releases remote-update worker and explicit update-now trigger** - `4067380` (feat)
2. **Task 2: Implement post-boot image confirmation and rollback bookkeeping** - `e07c921` (feat)
3. **Task 3: Update docs and validation flow for the full OTA lifecycle** - `670d98a` (docs)
4. **Task 4: Confirm the full OTA lifecycle on a real device** - approved human-verification checkpoint; no code changes required

## Files Created/Modified

- `app/src/ota/ota.c` - Adds GitHub release metadata fetch, HTTPS asset download, delayed confirmation, rollback reconciliation, and remote OTA work scheduling.
- `app/src/ota/ota.h` - Extends OTA runtime state with remote-work and confirmation hooks.
- `app/src/ota/ota_github_ca_certs.h` - Embeds the CA roots needed for GitHub API and release-asset HTTPS access.
- `app/src/app/bootstrap.c` - Notifies the OTA service once the app reaches `APP_READY`.
- `app/src/persistence/persistence_types.h` - Adds OTA states and result codes for pending confirmation, remote checks, confirmation, and rollback.
- `app/src/persistence/persistence.c` - Validates and names the new OTA result and state values.
- `app/src/panel/panel_http.c` - Adds the authenticated `POST /api/update/remote-check` route and richer OTA snapshot text for pending confirmation and rollback.
- `app/src/panel/panel_status.c` - Expands compact OTA summary truth with remote job state and pending-confirm warnings.
- `app/src/panel/assets/main.js` - Adds the `Update now` UI flow and remote OTA state badges.
- `README.md` - Documents the full Phase 8 OTA workflow and hardware verification checklist.
- `scripts/common.sh` - Adds Phase 8 ready markers, curl commands, scenario labels, and device checklist helpers.
- `scripts/validate.sh` - Prints the build-first Phase 8 OTA verification flow and exits cleanly after the checklist output.
- `scripts/build.sh` - Keeps optional artifact discovery from causing a false non-zero exit after a successful sysbuild.

## Decisions Made

- Kept GitHub-specific selection logic inside `ota_service` so the panel surface stays thin, authenticated, and transport-agnostic.
- Treated `APP_READY` plus the configured stable window as the only confirmation boundary for a test image, preserving MCUboot rollback until that point.
- Corrected the repo’s validation scripts to use the signed image the build actually emits, `zephyr.signed.bin`, for local OTA upload guidance.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Repaired the canonical validation path to exit cleanly after successful sysbuild output**
- **Found during:** Task 3 (Update docs and validation flow for the full OTA lifecycle)
- **Issue:** `scripts/build.sh` could return a false non-zero exit after a successful build because it ended on an optional missing-artifact probe, which in turn caused `./scripts/validate.sh` to report failure even when the firmware built and artifacts were generated.
- **Fix:** Made the artifact logging loop explicitly conditional and updated the OTA upload guidance to use `zephyr.signed.bin`, which the repo build reliably emits.
- **Files modified:** `scripts/build.sh`, `scripts/common.sh`, `README.md`
- **Verification:** `./scripts/validate.sh`
- **Committed in:** `670d98a`

**2. [Rule 3 - Workflow] Continued 08-03 locally after the executor runtime failed before producing any 08-03 work**
- **Found during:** Execute-phase orchestration
- **Issue:** The wave-3 executor hit a runtime usage-limit error before it created any `08-03` commits or summary, which would have left the phase blocked despite a clean repo state.
- **Fix:** Resumed the plan locally from the clean post-08-02 checkpoint, preserved atomic task commits, and recorded the checkpoint state in `.planning/STATE.md` before final approval.
- **Files modified:** `.planning/STATE.md`
- **Verification:** `git log --oneline --grep='08-03'`
- **Committed in:** `10e0723`

---

**Total deviations:** 2 auto-fixed (1 blocking, 1 workflow)
**Impact on plan:** No product-scope change. The fixes only restored the intended validation and execution flow.

## Verification

- `./scripts/build.sh` — Passed; sysbuild completed and produced `merged.hex`, `zephyr.signed.bin`, and `zephyr.signed.hex`.
- `./scripts/validate.sh` — Passed with `EXIT:0`; printed the full Phase 8 OTA checklist after a successful build.
- `rg -n "github|releases|latest stable|downloader|http_client|update now|daily" app/Kconfig app/prj.conf app/src/app/app_config.h app/src/ota/ota.h app/src/ota/ota.c app/src/panel/panel_http.c app/src/panel/assets/main.js` — Passed.
- `rg -n "boot_write_img_confirmed|boot_is_img_confirmed|APP_READY|stable|rollback|last_attempt|pending" app/src/app/app_config.h app/src/app/bootstrap.c app/src/ota/ota.h app/src/ota/ota.c app/src/persistence/persistence.h app/src/persistence/persistence_types.h app/src/persistence/persistence.c app/src/panel/panel_status.h app/src/panel/panel_status.c` — Passed.
- `bash -n scripts/validate.sh scripts/common.sh scripts/build.sh scripts/flash.sh` — Passed.
- `rg -n "Phase 8|update|upload|GitHub|rollback|confirm|signed\\.bin|curl" README.md scripts/common.sh scripts/validate.sh scripts/build.sh scripts/flash.sh` — Passed.
- Human checkpoint approved with local upload staging, same-version rejection, downgrade rejection, explicit apply reboot, post-boot confirmation, rollback visibility, GitHub update-now, daily retry, and panel truthfulness recorded.
- Checkpoint evidence: user responded `approved` after the blocking browser/curl/device verification flow.

## Human Verification

- **Checkpoint:** `checkpoint:human-verify`
- **Approval:** Approved on 2026-03-10 after real-device browser, curl, and OTA lifecycle verification.
- **Scenario checklist passed:** `local upload staging`, `same-version rejection`, `downgrade rejection`, `explicit apply reboot`, `post-boot confirmation`, `rollback visibility`, `github update now`, `daily retry`, and `panel truthfulness`.
- **Recorded in:** `README.md`, `scripts/common.sh`, `scripts/validate.sh`, and `.planning/phases/08-ota-lifecycle/08-VALIDATION.md`.

## Next Phase Readiness

- Task 4 is complete via approved real-device verification.
- Phase 8 is ready for phase-goal verification and completion routing.
- The repo now contains both the automated build-first validation path and the approved irreversible OTA lifecycle evidence needed to accept the phase.

## Self-Check: PASSED

- Verified `.planning/phases/08-ota-lifecycle/08-03-SUMMARY.md` exists.
- Verified task commits `4067380`, `e07c921`, and `670d98a` exist in git history.
- Verified `./scripts/validate.sh` exits `0`.
- Verified `.planning/STATE.md` records the approved checkpoint handoff prior to phase closeout.

---
*Phase: 08-ota-lifecycle*
*Completed: 2026-03-10*
