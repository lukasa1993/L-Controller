# Phase 8: OTA Lifecycle - Context

**Gathered:** 2026-03-10
**Status:** Ready for planning

<domain>
## Phase Boundary

Phase 8 adds safe firmware update lifecycle support for both local upload and remote pull. It must keep the existing authenticated local-LAN panel contract, stage updates through a bootloader-backed rollback-capable flow, and only make a new image permanent after a healthy post-update boot. It does not expand into broader release-engineering automation, multi-source release management, or a generalized deployment service.

</domain>

<decisions>
## Implementation Decisions

### Update surface and local operator flow
- OTA management should live in a dedicated update surface, with the existing dashboard update card acting as a summary and entry point.
- Local firmware upload should validate and stage first, then wait for an explicit operator apply action rather than rebooting immediately.
- The apply action should use a single confirmation dialog that summarizes the staged build and warns that the panel will disconnect during reboot.
- While a local image is staged and waiting to boot, the rest of the panel should remain usable, but it must show a strong pending-update warning.

### GitHub-based remote update flow
- Remote pull is fixed to the public GitHub Releases feed for `lukasa1993/L-Controller`; arbitrary remote sources are not part of Phase 8.
- The device should consider only the latest stable published GitHub Release; drafts and prereleases are excluded.
- Remote update checks do not require stored remote credentials because the GitHub source is public.
- The web surface must include an explicit `Update now` action that forces an immediate GitHub check/update cycle.
- The normal remote-update path should run daily, automatically check GitHub Releases, and when a newer stable release is found it should download, stage, and apply it automatically.

### Acceptance, confirmation, and rollback contract
- Local upload and GitHub remote pull must obey the same image-safety rules; neither path may bypass staging or rollback protections.
- OTA should accept only strictly newer firmware versions; same-version reinstall and downgrade flows are rejected in Phase 8.
- A newly booted firmware image becomes permanent only after it reaches the normal `APP_READY` contract and then survives a short stable window.
- The panel/API must show the currently running version, the staged or last-attempted version, the last update result, and explicit rollback status or reason.
- If the daily GitHub flow fails or rolls back, the device should try again on the next daily cycle rather than pausing indefinitely.

### Claude's Discretion
- Exact wording, layout, and information density of the dedicated update surface and dashboard summary card.
- Exact shape of the update status payload and how the current-versus-last-attempt data is rendered in the panel.
- Exact stable-window duration and the internal hook used to convert healthy boot into image confirmation.
- Exact GitHub Releases query mechanics, asset naming expectations, and how the device maps release metadata to the chosen firmware artifact.

</decisions>

<specifics>
## Specific Ideas

- “It must get it from `https://github.com/lukasa1993/L-Controller` releases.”
- “It must grab latest from GitHub Releases.”
- “We must have button in web where I can click to explicitly update NOW.”
- “In general it must check and update daily for normal flow.”

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `app/src/panel/panel_status.c`, `app/src/panel/assets/main.js`, and `app/src/panel/assets/index.html` already reserve an update card and placeholder contract that Phase 8 can turn into a real OTA summary plus entry point.
- `app/src/panel/panel_http.c` already provides authenticated exact-path JSON routes, static asset serving, cookie handling, and protected-route helpers that OTA endpoints should reuse.
- `app/CMakeLists.txt` already has the gzip asset pipeline needed for a richer dedicated update surface.
- `app/src/persistence/persistence.h`, `app/src/persistence/persistence_types.h`, and `app/src/persistence/persistence.c` already centralize NVS-backed typed persistence and schema-version handling.
- `app/pm_static.yml` already defines `settings_storage`, which is the existing durable store seam for OTA settings or retained OTA state.

### Established Patterns
- Prior phases locked the product to authenticated local-LAN HTTP only, with RAM-only sessions and protected exact routes.
- New services are added through `app_context` and initialized from `app_boot()` after config and persistence load.
- Runtime mutations already favor transactional behavior: persist first, then reload or publish live state, and roll back user-visible state if persistence fails.
- Recovery and scheduling already use a “recover once, then degrade safely” posture instead of reboot-looping forever.
- The existing boot contract and later-phase logic already treat `APP_READY` plus post-boot stability as meaningful lifecycle markers.

### Integration Points
- `app/src/ota/ota.h` already reserves an `ota_service` seam, but there is no implementation, `app_context` member, or bootstrap wiring yet.
- `app/src/app/app_context.h` and `app/src/app/bootstrap.c` are the obvious places to wire OTA runtime state, startup behavior, and confirmation hooks.
- `app/src/panel/panel_status.c` should grow from a placeholder-only `update` object into a real OTA summary with current version, staged/attempted version, and rollback state.
- `app/src/panel/panel_http.c` should grow authenticated `/api/update/*` routes that mirror the existing thin-handler pattern used for relay and scheduler flows.
- The current HTTP request-body buffers and JSON-only request handling are far too small for firmware upload, so Phase 8 must add a large-transfer upload path instead of reusing the existing small JSON mutation pattern directly.
- Current tracked source and generated build config show no active MCUboot/DFU/download stack yet, so Phase 8 planning must cover sysbuild, partitioning, download, staging, and confirm/rollback plumbing explicitly.

</code_context>

<deferred>
## Deferred Ideas

- GitHub-side release packaging, asset preparation, and automatic semantic-version increment tooling are adjacent release-engineering work, not part of the device OTA lifecycle contract for Phase 8.
- Multi-channel release selection, prerelease consumption, and non-GitHub remote sources stay out of Phase 8.
- A richer multi-entry OTA history or broader deployment-management surface can come later; Phase 8 only needs clear current-versus-last-attempt visibility.

</deferred>

---
*Phase: 08-ota-lifecycle*
*Context gathered: 2026-03-10*
