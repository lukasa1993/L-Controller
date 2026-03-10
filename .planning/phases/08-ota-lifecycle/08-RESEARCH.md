# Phase 8: OTA Lifecycle - Research

**Researched:** 2026-03-10
**Status:** Ready for planning

<focus>
## Research Goal

Define the smallest repo-aligned implementation shape that adds safe firmware updates for local upload and GitHub-based remote pull without breaking the existing local-LAN control model, recovery posture, or persistence ownership.

Phase 8 should reuse the existing authenticated panel shell, stage updates through MCUboot-backed secondary-slot writes, reject same-version and downgrade attempts, and confirm a new image only after the new firmware reaches the normal `APP_READY` path and survives a short stable window.

Phase 8 should not expand into general release-engineering automation, arbitrary remote-update sources, multi-channel rollout management, or a broader deployment service.

</focus>

<findings>
## Key Findings

### The repo has an OTA seam, but it is only a stub today

- `app/src/ota/ota.h` only defines `struct ota_service` plus `ota_service_init(...)`; there is no implementation, no `app_context` member, no bootstrap wiring, and no `ota.c` in `app/CMakeLists.txt`.
- `app/src/app/app_context.h` and `app/src/app/bootstrap.c` already show the house pattern for new runtime services: add the subsystem by value to `app_context`, initialize it during `app_boot()`, and keep panel/status layers as thin consumers of runtime truth.
- Planning implication: Phase 8 should create one real OTA service boundary early so local upload, remote pull, apply, confirm, rollback reporting, and panel status all flow through the same owner instead of growing separate code paths.

### Partitioning and build workflow are the first hard gate

- `app/sysbuild.conf` currently enables only `SB_CONFIG_WIFI_NRF70=y`; it does not enable MCUboot.
- `scripts/build.sh` still runs a single-image `west build` flow, and `scripts/flash.sh` still assumes a single app image build directory.
- `app/pm_static.yml` currently reserves only `app` plus `settings_storage`; there is no MCUboot bootloader, pad, or secondary-slot partition in the tracked project layout.
- The current generated build state matches that repo shape: `CONFIG_BOOTLOADER_MCUBOOT`, `CONFIG_DFU_TARGET`, `CONFIG_HTTP_CLIENT`, and `CONFIG_STREAM_FLASH` are not enabled in the built app configuration.
- The vendored Zephyr sysbuild sample documents that `SB_CONFIG_BOOTLOADER_MCUBOOT=y` is the switch that brings MCUboot into a sysbuild flow.
- Planning implication: 08-01 must treat sysbuild, static partitioning, and developer scripts as one unit of work. There is no meaningful OTA work until the repo can build, flash, and validate a bootloader-backed dual-slot image layout.

### The MCUboot and DFU APIs needed for staged updates already exist in the vendored SDK

- Zephyr provides `flash_img_init()`, `flash_img_buffered_write()`, and `flash_img_bytes_written()` in `zephyr/dfu/flash_img.h` for writing a staged image into the upload slot.
- Zephyr MCUboot APIs in `zephyr/dfu/mcuboot.h` provide `boot_read_bank_header()`, `boot_is_img_confirmed()`, `boot_request_upgrade()`, and `boot_write_img_confirmed()`.
- Nordic also ships `dfu_target_mcuboot.h` and the `downloader` library, which can stream downloaded fragments into an application-defined pipeline.
- Planning implication: both local upload and remote pull should converge on one shared OTA staging path that writes the secondary slot, reads staged-image metadata, enforces the “strictly newer only” rule, requests a test upgrade, and later confirms the booted image. Avoid building two separate validation or slot-management stacks.

### The authenticated panel is ready for OTA control, but not for firmware-sized request bodies

- `app/src/panel/panel_http.c` already has authenticated exact-path routes, cookie handling, and protected GET/POST handlers that Phase 8 should reuse.
- The current request model is intentionally small and JSON-shaped: route-local request buffers are 160, 64, or 256 bytes, and handlers accumulate the full body before parsing it.
- `app/src/panel/panel_status.c`, `app/src/panel/assets/index.html`, and `app/src/panel/assets/main.js` already reserve an update card, but that contract is still an explicit placeholder.
- Planning implication: 08-02 should add a dedicated authenticated OTA route family, but it must not reuse the current “buffer the whole request and then parse JSON” pattern for image upload. Firmware upload needs a streaming handler that consumes request chunks incrementally and writes them into the staging pipeline as they arrive.

### The current persistence boundary can probably absorb OTA state without resetting prior user data

- The persistence layer stores auth, actions, relay, and schedules as separate NVS entries (`0x4001` through `0x4004`) with section-local validation and save APIs.
- Existing sections are not packed into one monolithic flash blob; each section is loaded and written independently through typed helpers.
- Inference from `app/src/persistence/persistence.c`: adding a new typed OTA persistence section can likely be done without forcing an auth/action/relay/schedule reset, as long as existing stored section shapes stay unchanged. A layout-version bump is only necessary if Phase 8 changes the existing section contracts, not merely because OTA adds a new durable section or new NVS key.
- Planning implication: Phase 8 should keep OTA state inside the existing typed persistence boundary, either as a new OTA section or an equally disciplined extension, so staged version, last attempt, remote policy, and rollback reason stay durable without introducing ad hoc raw NVS ownership.

### There is no current firmware-version authority in the product

- The repo has persistence schema versions, but no product-image version contract surfaced through the panel or OTA code because OTA does not exist yet.
- MCUboot image headers already contain semantic-version data, and the vendored SDK shows `boot_read_bank_header()` being used to extract current image versions.
- Planning implication: Phase 8 needs one version authority and comparison rule shared by both local upload and remote pull. The cleanest source is the MCUboot image header read from the running slot and the staged slot, not an unrelated app-defined string.

### Remote pull should be an app-owned GitHub workflow, not a generic cloud FOTA abstraction

- The locked product decision is narrow: remote update source is only the public GitHub Releases feed for `lukasa1993/L-Controller`, only the latest stable release is eligible, and the panel must expose an explicit `Update now` action.
- The repo already has Wi-Fi, DNS, sockets, and DHCP/NTP pieces, but no current HTTP client or download configuration in `app/prj.conf`.
- Nordic’s downloader library is present and fragment-oriented; Zephyr’s HTTP client API is also present in the vendored SDK. Both are a better fit for a repo-owned GitHub Releases workflow than bolting in a broader cloud-FOTA abstraction.
- Planning implication: 08-03 should implement a repo-owned remote-update worker that fetches release metadata, selects the expected artifact, streams it through the same OTA staging pipeline used by local upload, and keeps GitHub-specific selection rules inside the OTA service rather than leaking them into the panel.

### Daily auto-update should be OTA-owned runtime work, not a visible schedule record

- Phase 7 already introduced trusted UTC time, a scheduler service, and a daily-capable timing model, but that system is operator-authored schedule infrastructure with explicit CRUD and relay-oriented action choices.
- The Phase 8 remote-update cadence is a fixed product behavior, not a user-authored automation.
- Planning implication: the OTA service should own its own daily check work item and use the same trusted-time and network-readiness posture the repo already established. Do not insert hidden OTA jobs into the operator-visible schedule table.

### Confirmation should depend on healthy post-boot uptime, not on internet reachability

- The product requirement says a new image becomes permanent only after a healthy post-update boot plus a short stable window.
- `app/src/app/bootstrap.c` already treats `APP_READY` as the ready-path milestone, and `app/src/recovery/recovery.c` already keeps retained reset breadcrumbs and a stable-window concept for recovery clearing.
- Inference from those existing contracts: image confirmation should key off “new firmware reached the normal app-ready path and stayed healthy for a short monotonic uptime window,” not on external internet reachability. A device that boots locally and serves the panel correctly should still be confirmable even if upstream internet is degraded.
- Planning implication: 08-03 needs boot-time OTA state inspection plus a delayed confirmation hook based on post-boot health, and it needs durable “last attempted image / last result / rollback reason” state so the next boot can explain what happened.

</findings>

<implementation_shape>
## Recommended Plan Shape

### 08-01 should establish the bootloader-backed foundation and shared OTA core

- Enable MCUboot through sysbuild and reshape `pm_static.yml` into a dual-slot layout that still preserves `settings_storage`.
- Update build and flash scripts so local development produces and flashes sysbuild artifacts instead of a single app image.
- Add a real `ota_service` plus shared staging helpers for slot writing, image-header reads, version comparison, apply-request plumbing, and durable OTA state/status structures.
- Keep this plan intentionally UI-light. Its job is to make Phase 8 physically possible and to give later plans a shared OTA runtime owner.

### 08-02 should deliver authenticated local upload, staged-image validation, and the update surface

- Add authenticated exact OTA routes for status, upload, apply, and cancel or clear-stage operations.
- Make the upload route streaming and binary-safe instead of full-body JSON buffering.
- Expose current version, staged version, last result, and pending-update state in the status payload and replace the placeholder update card with a dedicated update surface.
- Keep the local upload contract explicit: stage first, validate and reject same-version or downgrade attempts, then wait for an explicit operator apply action rather than rebooting immediately.

### 08-03 should add GitHub remote pull, confirm/rollback handling, and the final validation flow

- Add a GitHub Releases poll/download worker plus the `Update now` action, all routed through the shared OTA stage-and-apply pipeline.
- Persist enough OTA attempt state to distinguish “staged,” “booting test image,” “confirmed,” and “rolled back” outcomes across reboot.
- Confirm the new image only after the booted firmware reaches `APP_READY` and survives a short stable uptime window; if confirmation never happens, let MCUboot revert and surface the rollback reason clearly on the next boot.
- Update repo docs and validation scripts so sysbuild artifact expectations, upload flow, remote GitHub flow, apply/reboot behavior, and rollback scenarios are all explicit.

</implementation_shape>

<validation>
## Validation Architecture

### Automated validation should stay build-first but become sysbuild-aware

- Keep `./scripts/build.sh` as the fastest automated signal and `./scripts/validate.sh` as the canonical repo-owned validation entrypoint.
- Phase 8 automated checks should prove the repo now builds the sysbuild + MCUboot shape:
  - MCUboot enabled in sysbuild config
  - partition map contains bootloader, pad, primary, secondary, and settings partitions
  - build output produces signed update artifacts for the secondary slot
  - OTA source files are in the build and no placeholder-only update status remains

### Structural checks should prove both update paths use the same safety pipeline

- Add `rg`-style plan-level checks that confirm:
  - local upload handlers delegate into `ota_service` rather than writing flash directly from the panel layer
  - staged-image version checks exist and reject same-version or downgrade attempts
  - remote GitHub download flows into the same OTA staging/apply path as local upload
  - boot confirmation uses MCUboot confirmation APIs instead of marking the image permanent at stage time
  - rollback or last-attempt status is persisted and surfaced through panel status

### Final approval must remain a blocking browser, curl, and device checkpoint

Minimum scenario set:

1. **Local upload staging** — an authenticated operator uploads a signed image, sees staged-version metadata, and the device does not reboot until explicit apply.
2. **Version gate enforcement** — same-version reinstall and downgrade attempts are rejected before apply.
3. **Apply and confirm** — applying a staged newer image reboots into the new firmware, the device reaches `APP_READY`, and the image becomes confirmed only after the stable window.
4. **Rollback visibility** — an unconfirmed or intentionally bad image reverts through MCUboot and the next boot surfaces rollback status and reason clearly.
5. **Remote update now** — the authenticated `Update now` path checks GitHub Releases, downloads the latest stable artifact, stages it, and applies it through the same safety rules.
6. **Daily retry behavior** — a failed or rolled-back remote attempt does not pause remote updates forever; the next daily cycle can retry.
7. **Panel truthfulness** — the panel/API show current version, staged or last-attempted version, last result, and rollback state consistently before and after reboot.

</validation>
