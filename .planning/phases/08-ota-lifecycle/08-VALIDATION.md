---
phase: 8
slug: ota-lifecycle
status: draft
nyquist_compliant: true
wave_0_complete: true
created: 2026-03-10
---

# Phase 8 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | other — build-first Zephyr sysbuild validation plus blocking browser/curl/device OTA sign-off |
| **Config file** | none — existing repo scripts plus sysbuild, MCUboot, and partition-manager config |
| **Quick run command** | `./scripts/build.sh` |
| **Full suite command** | `./scripts/validate.sh` |
| **Estimated runtime** | ~90 seconds |

---

## Sampling Rate

- **After every task commit:** Run `./scripts/build.sh`
- **After every plan wave:** Run `./scripts/validate.sh`
- **Before `$gsd-verify-work`:** Full automated validation must be green, then complete the blocking browser/curl/device OTA scenarios on real hardware
- **Max feedback latency:** 90 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 08-01-01 | 01 | 1 | OTA-03 | sysbuild + partitions | `rg -n "BOOTLOADER_MCUBOOT|mcuboot|secondary|pad|settings_storage|sysbuild" app/sysbuild.conf app/prj.conf app/pm_static.yml scripts/build.sh scripts/flash.sh` | ❌ planned | ⬜ pending |
| 08-01-02 | 01 | 1 | OTA-03, OTA-04 | ota core wiring | `test -f app/src/ota/ota.c && rg -n "flash_img|boot_read_bank_header|boot_request_upgrade|boot_is_img_confirmed|ota_service|staged|version" app/src/ota/ota.h app/src/ota/ota.c app/src/app/app_context.h app/src/app/bootstrap.c` | ❌ planned | ⬜ pending |
| 08-01-03 | 01 | 1 | OTA-03 | shared build signal | `./scripts/build.sh && rg -n "signed\\.bin|signed\\.hex|mcuboot_secondary_app|build" scripts/build.sh scripts/flash.sh` | ✅ existing | ⬜ pending |
| 08-02-01 | 02 | 2 | OTA-01, OTA-03 | upload route | `rg -n "/api/update|upload|apply|clear|cancel|content-length|application/octet-stream|ota_service" app/src/panel/panel_http.c app/src/ota/ota.c app/src/ota/ota.h` | ❌ planned | ⬜ pending |
| 08-02-02 | 02 | 2 | OTA-01, OTA-03 | update status + UI | `rg -n "update|currentVersion|stagedVersion|lastResult|rollback|upload|apply" app/src/panel/panel_status.c app/src/panel/panel_status.h app/src/panel/assets/index.html app/src/panel/assets/main.js` | ✅ existing | ⬜ pending |
| 08-02-03 | 02 | 2 | OTA-01, OTA-03 | version safety rules | `rg -n "same-version|downgrade|newer|sem_ver|boot_read_bank_header|reject" app/src/ota/ota.c app/src/panel/panel_http.c` | ❌ planned | ⬜ pending |
| 08-03-01 | 03 | 3 | OTA-02, OTA-03 | GitHub remote pull | `rg -n "github|releases|latest stable|downloader|http_client|update now|daily" app/src/ota/ota.c app/src/panel/panel_http.c app/src/panel/assets/main.js app/Kconfig app/prj.conf` | ❌ planned | ⬜ pending |
| 08-03-02 | 03 | 3 | OTA-03, OTA-04 | confirm + rollback | `rg -n "boot_write_img_confirmed|boot_is_img_confirmed|APP_READY|stable|rollback|last_attempt|pending" app/src/ota/ota.c app/src/app/bootstrap.c app/src/panel/panel_status.c app/src/persistence/persistence.c app/src/persistence/persistence_types.h` | ❌ planned | ⬜ pending |
| 08-03-03 | 03 | 3 | OTA-01, OTA-02, OTA-03, OTA-04 | docs + validation | `bash -n scripts/validate.sh scripts/common.sh scripts/build.sh scripts/flash.sh && rg -n "Phase 8|update|upload|GitHub|rollback|confirm|signed\\.bin|curl" README.md scripts/common.sh scripts/validate.sh scripts/build.sh scripts/flash.sh` | ✅ existing | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [x] Existing script-based validation remains the canonical automated signal
- [x] No new external test framework is required before Phase 8 execution begins
- [x] Build and flash scripts must become sysbuild-aware in Plan 01 so the automated path reflects the real OTA artifact shape
- [x] Final approval remains blocked on real-device OTA scenarios that automated build checks cannot prove

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Authenticated operator can upload a signed firmware image locally and see it staged without immediate reboot | OTA-01 | Requires browser/API upload behavior, staged metadata visibility, and live device flash writes | Log in through the panel or curl, upload a newer signed image, confirm the device reports staged version metadata, and confirm no reboot occurs until the explicit apply action is used. |
| Same-version reinstall and downgrade attempts are rejected before staging or apply | OTA-01, OTA-03 | Requires signed image artifacts with controlled version differences and operator-visible failure behavior | Attempt one same-version upload and one older-version upload, then confirm both are rejected with clear panel/API feedback and no staged image remains eligible for apply. |
| Applying a staged newer image reboots into the new firmware and confirms only after the healthy boot window | OTA-03, OTA-04 | Requires real reboot, bootloader swap behavior, and post-boot stability observation | Stage a newer signed image, trigger apply, watch the reboot, confirm the new firmware reaches `APP_READY`, and verify the image is only marked permanent after the documented stable window passes. |
| A non-confirmed image rolls back cleanly and the next boot reports rollback reason or last failure truthfully | OTA-03, OTA-04 | Requires real MCUboot revert behavior across reboots that build checks cannot simulate faithfully | Trigger an update path that intentionally avoids confirmation or uses a known-bad image, then confirm the device returns to the previous firmware and the panel/API show rollback or failed-attempt information on the next boot. |
| The update surface shows current version, staged or last-attempted version, last result, and rollback state before and after reboot | OTA-01, OTA-03, OTA-04 | Requires real status refresh around staging, reboot, and rollback edges | Refresh the panel/API before stage, after stage, after apply, and after the post-boot window, then confirm every field stays internally consistent and operator-meaningful. |
| `Update now` checks GitHub Releases, fetches the latest stable release, and stages or applies it through the same safety rules | OTA-02, OTA-03 | Requires outbound network access, GitHub response handling, and live device download behavior | On a device with internet access, trigger `Update now`, confirm the GitHub source is queried, confirm the expected artifact is downloaded, and verify the resulting staged/apply path follows the same rules as local upload. |
| Daily GitHub auto-update retries after failure or rollback instead of pausing indefinitely | OTA-02, OTA-03, OTA-04 | Requires time passage or controlled schedule simulation plus repeated remote failure observation | Force or simulate one failed remote update cycle, then allow the next daily check window and confirm the device attempts the GitHub flow again instead of disabling remote updates permanently. |

*If none: "All phase behaviors have automated verification."*

---

## Validation Sign-Off

- [x] All tasks have `<automated>` verify or existing Wave 0 infrastructure
- [x] Sampling continuity: no 3 consecutive tasks without automated verify
- [x] Wave 0 covers all missing framework dependencies
- [x] No watch-mode flags
- [x] Feedback latency < 90s
- [x] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
