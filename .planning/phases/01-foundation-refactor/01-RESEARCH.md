# Phase 1: Foundation Refactor - Research

**Researched:** 2026-03-08
**Status:** Ready for planning

<focus>
## Research Goal

Identify the cleanest way to turn the current single-file Zephyr/Nordic Wi-Fi bring-up app into a responsibility-oriented firmware layout that preserves the existing build flow and hardware-ready baseline while establishing durable module homes for later phases.

</focus>

<findings>
## Key Findings

### Current extraction seams are already visible inside `main.c`

- `app/src/main.c` already contains separable responsibilities rather than one inseparable control loop.
- The strongest extraction seams are:
  - configuration translation: `app_security_from_kconfig()`
  - Wi-Fi/interface observation: `log_iface_status()`
  - event handling: `handle_wifi_connect_result()`, `handle_wifi_disconnect_result()`, `handle_ipv4_dhcp_bound()`, `wifi_event_handler()`, `ipv4_event_handler()`, `wifi_ready_cb()`
  - network lifecycle orchestration: `register_callbacks()`, `wait_for_wifi_ready_state()`, `connect_wifi_once()`, `wait_for_wifi_connection_and_ipv4()`
  - post-connect health probe: `run_reachability_check()`
  - application bootstrap: `main()`
- This means Phase 1 does not need speculative architecture work first; it can extract real production seams from existing behavior.

### The repo already supports a firmware-only refactor without changing product behavior

- `app/CMakeLists.txt`, `app/Kconfig`, and `app/prj.conf` already define a standard Zephyr application shell, so the refactor can stay inside the current app boundary.
- `scripts/build.sh`, `scripts/flash.sh`, `scripts/console.sh`, and `scripts/doctor.sh` already form the operational workflow; Phase 1 should preserve them rather than inventing a new developer flow.
- The current build-time contract is already explicit through Kconfig values like `CONFIG_APP_WIFI_SSID`, `CONFIG_APP_WIFI_PSK`, `CONFIG_APP_WIFI_SECURITY`, `CONFIG_APP_WIFI_TIMEOUT_MS`, and `CONFIG_APP_REACHABILITY_HOST`. Phase 1 should carry that contract forward instead of introducing runtime config storage prematurely.

### Recommended module map: responsibility-first with moderate nesting

The best fit for the repo is a responsibility-oriented tree under `app/src/` with minimal but clear nesting:

- `app/src/app/`
  - bootstrap/composition entrypoints
  - shared app-level config and typed context definitions
- `app/src/network/`
  - Wi-Fi lifecycle, event handling, interface status, and reachability logic
- `app/src/recovery/`
  - placeholder home for watchdog/restart policy work in later phases
- `app/src/panel/`
  - placeholder home for future local HTTP/admin surface
- `app/src/actions/`
  - placeholder home for relay-triggered behavior orchestration
- `app/src/scheduler/`
  - placeholder home for timed execution logic
- `app/src/persistence/`
  - placeholder home for retained settings/state storage
- `app/src/ota/`
  - placeholder home for firmware update flow
- `app/src/relay/`
  - placeholder home for physical relay control

This structure satisfies the roadmap intent without over-layering the codebase. It also matches the user’s explicit guidance to optimize for durable long-term ownership, not a migration staging area.

### `main.c` should end as composition only

- `main()` should retain only:
  - top-level logging/boot message
  - creation or acquisition of the application context/config
  - invocation of a bootstrap function such as `app_boot_run()`
  - final idle loop if that loop remains the right ownership boundary
- All Wi-Fi event registration, connection sequencing, DHCP waiting, and reachability logic should move behind subsystem entry points.
- `main.c` should not keep hidden lifecycle state, callback bodies, or network business logic after Phase 1.

### Shared state should move from file-static globals to explicit typed structures

The current state is spread across file-static variables:

- `wifi_iface`
- callback registrations
- semaphores
- `wifi_ready`, `wifi_connected`, `ipv4_bound`
- `connect_status`, `last_disconnect_status`
- `leased_ipv4`

For Phase 1, these should be reorganized into explicit types, for example:

- `struct app_config` for immutable boot/config inputs derived from Kconfig
- `struct network_runtime_state` for Wi-Fi readiness, connection result, DHCP lease, and disconnect metadata
- `struct app_context` as the top-level shared struct passed between bootstrap-owned subsystems

The key rule is not merely “put globals in a struct”; the struct ownership must be narrow. Networking state should be owned by the networking subsystem and exposed intentionally, not through a catch-all global header.

### Build implications are straightforward and low risk

- `app/CMakeLists.txt` currently compiles only `src/main.c`; Phase 1 will need to register multiple source files and likely add a small set of include directories.
- No evidence suggests Phase 1 needs to change `app/sysbuild.conf` or the baseline NCS workspace setup.
- `app/Kconfig` can likely remain the public app configuration surface for now, though typed config-loading helpers should move into source modules.
- The refactor should avoid unnecessary churn in `scripts/` unless validation scaffolding genuinely benefits from one new helper entrypoint.

### The main technical risk is over-scaffolding instead of extracting real seams

The user explicitly allows broad future scaffolding, but the phase success criteria are about real modularization and explicit interfaces, not placeholder-only folders.

The biggest failure modes are:

- creating many empty directories without extracting the real networking/bootstrap behavior
- leaving callback-driven state as pseudo-global variables shared across files
- moving logic out of `main.c` mechanically while keeping `main.c` as the de facto orchestration owner through thin wrappers
- introducing a large new testing framework that delays the refactor without materially reducing phase risk

</findings>

<recommendation>
## Recommended Refactor Direction

### Preferred architectural split

Phase 1 should center on three concrete implementation layers inside the current app:

1. **App bootstrap layer**
   - Owns boot sequencing and top-level orchestration.
   - Converts `main.c` into a tiny composition root.

2. **Network bring-up layer**
   - Owns Wi-Fi event callbacks, connection attempts, DHCP waiting, interface status, and reachability verification.
   - Keeps mutable connection state inside subsystem-owned typed structures.

3. **Future subsystem homes**
   - Introduce clear directories and minimal placeholder headers for recovery, panel, actions, scheduler, persistence, OTA, and relay.
   - These placeholders should establish ownership boundaries without pretending later phases are implemented.

### Interface guidance

- Prefer small headers exposing explicit APIs such as initialization, connect/wait, and status accessors.
- Keep Zephyr-specific types at subsystem boundaries where practical; do not leak low-level networking details everywhere.
- Keep reachability checking as part of the network subsystem for now, because it is part of the current ready-state contract.
- Avoid a shared “utils” dumping ground; every extracted function should land in an owned module.

</recommendation>

<validation>
## Validation Architecture

### Existing validation assets to reuse

- `scripts/doctor.sh` already validates host prerequisites, SDK workspace presence, and hardware visibility.
- `scripts/build.sh` is the canonical build regression command.
- `scripts/flash.sh` plus `scripts/console.sh` already define the hardware smoke path.
- Runtime logs already expose the effective happy-path markers:
  - `Wi-Fi connected`
  - `DHCP IPv4 address: ...`
  - `Reachability check passed`
  - `APP_READY`

### Recommended Phase 1 validation strategy

- **Fast automated gate:** preserve or add a single repeatable command that proves the refactored tree still configures and builds, using `./scripts/build.sh` as the canonical build signal.
- **Preflight gate:** use `./scripts/doctor.sh` before build-heavy or hardware-bound validation to surface missing toolchain/device prerequisites early.
- **Manual hardware sign-off:** keep `./scripts/flash.sh` and `./scripts/console.sh` as the final required sign-off because the phase success criteria explicitly depend on real bring-up behavior.
- **Lightweight script hygiene:** if Phase 1 adds or edits shell helpers, run `shellcheck` where available because the existing scripts are already written with ShellCheck-friendly annotations.

### Nyquist implications

- The repo does not yet justify introducing a large firmware test framework in this phase.
- A Nyquist-compliant strategy for Phase 1 should therefore focus on:
  - frequent build feedback
  - explicit mapping from each plan to either automated build checks or clearly declared manual-only verification
  - no long stretches of refactor work without a repeatable command proving the app still builds
- If the refactor extracts pure helper logic cleanly, Phase 1 can optionally add narrow host-runnable checks later, but that should remain subordinate to the build-and-hardware baseline.

</validation>

<planning_implications>
## Planning Implications

- The roadmap’s planned three-plan split is sound and should be preserved.
- Plan 1 should establish the module map, shared typed interfaces, and future subsystem homes.
- Plan 2 should perform the real extraction of Wi-Fi bring-up/runtime flow from `main.c` into the new bootstrap/network modules.
- Plan 3 should add validation scaffolding and sign-off guidance around the refactored structure without overreaching into a brand-new test framework.
- Plan 2 should depend on Plan 1.
- Plan 3 can depend on Plan 1 and focus on build/hardware validation scaffolding; whether it also depends on Plan 2 should be determined by the exact validation artifacts chosen.

</planning_implications>

---
*Phase: 01-foundation-refactor*
*Research completed: 2026-03-08*
