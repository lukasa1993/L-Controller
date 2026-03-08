# Phase 1: Foundation Refactor - Context

**Gathered:** 2026-03-08
**Status:** Ready for planning

<domain>
## Phase Boundary

Replace the single `main.c` application structure with modular subsystem boundaries while preserving the repo’s working Zephyr/Nordic application shape and restoring baseline bring-up behavior by phase sign-off. This phase is about establishing the long-term firmware foundation, not adding new end-user capabilities.

</domain>

<decisions>
## Implementation Decisions

### Boundary strictness
- `main.c` should end Phase 1 as bootstrap/composition only, not a runtime business-logic owner.
- Hard internal rewrites are acceptable during the phase; there is no requirement to preserve today’s exact internal flow just because it already exists.
- Temporary compatibility glue should be minimal, and transitional wrappers should not remain as the long-term answer for real implemented modules.
- Real implemented modules should end with narrow ownership and explicit interfaces.
- Phase 1 may touch firmware layout, build/config wiring, and even developer workflow scripts if that is genuinely needed to establish the foundation cleanly.
- Future subsystem structure should be created now; broad scaffolding is acceptable for placeholders, but strict boundaries for the actually implemented seams take priority.
- Placeholder depth can be broad: full empty skeletons for future subsystem homes are acceptable if they clarify where later work belongs.

### Code organization style
- Organize the firmware primarily by responsibility, not by abstract technical layers alone.
- Use moderate nesting: subsystem folders are welcome, but the tree should still be fast to navigate.
- Concrete domains can keep short/generic names where that stays clear.
- Policy/orchestration modules should use more explicit role names when short names would blur ownership.
- The resulting tree should read like the long-term home of the product, not like a temporary migration staging area.

### Behavior lock and sign-off bar
- Exact preservation of the current single-file implementation is not a goal; a hard rewrite is acceptable.
- Phase 1 can iterate build-first most of the time rather than requiring constant on-device validation.
- Even with build-led iteration, the fixed sign-off gate is that the restructured firmware must again build, flash, boot, and regain the baseline ready state on hardware.
- Phase 1 should leave behind both working code and a clear module map for future phases.
- The foundation should feel production-grade by the end of the phase, not merely “good enough to keep going.”

### Claude's Discretion
- Exact folder names within each subsystem as long as they follow the responsibility-first, moderate-nesting guidance.
- Which future subsystem stubs are header-only versus lightly wired, as long as the module homes are obvious.
- How much build/script churn is actually necessary if the foundation can be established cleanly with smaller changes.
- Exact validation scaffolding beyond build-led iteration plus final hardware sign-off.

</decisions>

<specifics>
## Specific Ideas

- “We do hard rewrites” — the user is not attached to the current single-file implementation and prefers a serious refactor over incremental patching.
- The codebase should be strict for real module boundaries, while still being willing to lay down broad future scaffolding in the same phase.
- The phase should optimize for a durable long-term foundation rather than preserving the current shape for comfort.

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `app/Kconfig`: already provides an app-specific configuration surface; it can remain the entry point while runtime-facing config types are split out behind modules.
- `app/prj.conf`: already encodes the working Zephyr/Nordic subsystem set and safety-oriented flags; Phase 1 should preserve this baseline while the source tree is reorganized.
- `scripts/build.sh` and `scripts/common.sh`: existing developer workflow wrappers already isolate the app root and build directory conventions, so internal source reorganization is feasible without redesigning the whole workflow by default.
- `app/src/main.c`: already contains natural extraction seams such as Wi-Fi callback handling, connection waiting, security translation, and the reachability check.

### Established Patterns
- Zephyr-standard app structure is already in place with `app/CMakeLists.txt`, `app/Kconfig`, `app/prj.conf`, and `app/sysbuild.conf`.
- Error handling is explicit and fail-fast, using negative errno returns plus `LOG_INF` / `LOG_WRN` / `LOG_ERR` severity meaningfully.
- Build-time configuration via Kconfig plus a local secrets overlay is an established pattern and should remain the basis for Phase 1.
- Host-side scripts centralize path/toolchain concerns in `scripts/common.sh`, which argues against needless duplication of workflow logic during the refactor.

### Integration Points
- `app/CMakeLists.txt` currently registers only `src/main.c`; Phase 1 will expand this into multi-file source registration.
- `main.c` is the current composition root and will become the natural bootstrap entry after logic is extracted.
- The current Wi-Fi lifecycle helpers and reachability path are the most immediate candidates for carving into well-owned modules.
- Future phases will plug into the structure created here, so folders and interfaces established in Phase 1 become the default homes for networking, recovery, configuration, panel, action, scheduler, OTA, and relay work.

</code_context>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope.

</deferred>

---
*Phase: 01-foundation-refactor*
*Context gathered: 2026-03-08*
