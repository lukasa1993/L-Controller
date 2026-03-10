# Phase 6: Action Engine & Relay Control - Research

**Researched:** 2026-03-10
**Status:** Ready for planning

<focus>
## Research Goal

Identify the smallest Zephyr-native implementation shape that turns the Phase 5 relay placeholder into a real, safe, local-LAN control path while keeping the operator experience relay-centric and routing all real work through one internal action dispatcher.

Phase 6 should not pull in schedule authoring UX, multi-relay support, a broader settings editor, or OTA/update workflows. It does need to leave behind a reusable `action_id` execution path for Phase 7, a real relay runtime with explicit startup policy, and a live status contract the panel can render truthfully.

</focus>

<findings>
## Key Findings

### The persistence layer already defines the narrowest useful Phase 6 data contract

- `persisted_action_catalog` already stores stable `action_id` values, enable flags, and the first relay on/off payload, while schedules already reference actions by `action_id`.
- `persisted_relay` already stores the remembered desired state and the configurable normal-boot reboot policy.
- Save/load validation is centralized in `persistence_store_save_*()` and already rejects invalid relay policy values and dangling schedule-to-action references.
- Planning implication: Phase 6 should keep the panel and future scheduler bound to `action_id` and reuse persistence APIs rather than allowing HTTP handlers or relay code to write durability state directly.

### Live relay behavior needs richer runtime state than the persisted snapshot can express

- The current Phase 5 `panel_status` payload only exposes persisted relay metadata plus placeholder copy.
- The locked Phase 6 UX requires actual/applied state, remembered desired state, source, safety or recovery note, and inline blocked or pending reason.
- Those are runtime concerns, not durable configuration. `persisted_relay` should remain the durable remembered-intent and reboot-policy input, while `relay_service` should own a richer live status surface in RAM.
- Planning implication: add a relay runtime status contract before panel command work so the first control surface debugs against real state rather than placeholder inference.

### Recovery already exposes the signal needed for stricter post-fault behavior

- `recovery_manager_last_reset_cause()` already preserves whether the most recent boot followed a recovery-triggered reset and why.
- The Phase 6 context requires recovery-triggered reboot or confirmed fault recovery to force the relay off until a fresh command arrives, even when the remembered desired state was on.
- That means startup policy evaluation must distinguish ordinary reboot from recovery reset and must not silently overwrite `last_desired_state` when safety forces the applied state off.
- Planning implication: relay startup policy should consume persisted relay config plus the retained recovery breadcrumb before the panel starts.

### Boot composition needs deliberate relay and action ownership before HTTP starts

- `app_boot()` currently loads config and persistence, initializes panel auth and recovery, starts network supervision, and then starts the panel HTTP service.
- There is no relay or action runtime in `app_context`, no `actions.c` or `relay.c` in the build, and no init slots for them in `bootstrap.c`.
- Planning implication: Phase 6 should compose `relay_service` and `action_dispatcher` into `app_context`, initialize them before `panel_http_server_init()`, and keep HTTP handlers thin.

### The physical relay needs a driver boundary and board-owned binding

- The repo has no `relay.c`, no `CONFIG_GPIO`, and no board overlay or devicetree alias for a first relay output.
- Because the roadmap explicitly says "ship the first safe relay on/off operation," Phase 6 cannot stay at placeholder or mock level. It needs a real board-owned binding for the current hardware target.
- The cleanest shape is a relay driver and service boundary backed by a board overlay alias such as `relay0`, keeping pin selection and polarity out of HTTP, action, and persistence code.
- Planning implication: isolate the hardware unknown to one small build and overlay task so the rest of the phase can stay generic.

### The panel surface should stay relay-centric externally and generic internally

- The existing Phase 5 server and frontend already use a small aggregate status API plus cookie-backed auth.
- The Phase 6 context explicitly wants a single relay toggle with inline pending and failure feedback while keeping action-engine terminology internal.
- That favors an authenticated relay-centric control route such as `POST /api/relay/desired-state` with a simple boolean body, while the handler internally maps to well-known `action_id` values and calls the dispatcher.
- Planning implication: the browser should submit desired relay state and re-render from live status, but it should never see GPIO semantics or raw action IDs.

### A clean device needs built-in relay actions without a setup step

- Phase 4 intentionally defaulted the action catalog to zero entries, which was safe before real control existed.
- Phase 6 now requires a clean device to control the first relay immediately.
- The least disruptive bridge is to seed or repair a pair of reserved built-in actions for relay off and relay on during dispatcher startup, using stable IDs and preserving any existing user-owned catalog entries when possible.
- Planning implication: the action engine should own reserved-action materialization and reuse persistence APIs so Phase 7 scheduling can target the same IDs.

### The existing persistence schema is acceptable if the generic contract stays behind the action boundary

- `persisted_action` is only generic in a narrow Phase 6 sense: it carries a stable ID and enable flag, but its current payload is still the first relay on or off outcome.
- ACT-01 can still be met in Phase 6 if `action_dispatcher` exposes a generic execute contract and hides relay-specific catalog details inside the action module.
- A full action-schema broadening is still possible later, but it would trigger the repo's current whole-layout version reset behavior.
- Planning implication: keep the public contract generic now and avoid a Phase 6 persistence migration unless execution proves it is necessary.

### Validation must stay build-first but add a blocking hardware safety checkpoint

- The repo still has no first-party test suite or CI harness; `./scripts/validate.sh` and `./scripts/build.sh` remain the only canonical automated signals.
- That is not enough to prove physical relay behavior, safe boot policy, recovery-forced off behavior, or actual-versus-desired mismatch visibility.
- Phase 6 needs the same pattern as Phases 4 and 5: fast automated build and structural checks during iteration, then a blocking browser, curl, and device checkpoint for real actuator and safety scenarios.
- Planning implication: the final plan should update `scripts/common.sh`, `scripts/validate.sh`, and `README.md` with Phase 6-specific curl commands, scenario labels, ready markers, and manual approval criteria.

</findings>

<implementation_shape>
## Recommended Implementation Shape

### Keep the relay service as the sole owner of applied hardware state

- Add `app/src/relay/relay.c` and expand `relay.h` into a real service contract with:
  - init and startup-policy application
  - read-only runtime status access
  - one apply and force-off path used by the action engine
- Keep the live relay status in RAM and expose fields that match the locked UX:
  - availability and implemented
  - actual applied state
  - remembered desired state
  - last change source
  - safety or recovery note
  - pending or blocked reason
- Preserve `persisted_relay.last_desired_state` as the durable remembered intent and `persisted_relay.reboot_policy` as the durable normal-boot policy input.

### Make startup policy evaluation explicit and side-effect-aware

- Recommended ordinary-boot behavior:
  - `SAFE_OFF` => actual relay off, remembered desired preserved
  - `RESTORE_LAST_DESIRED` => actual relay follows remembered desired
  - `IGNORE_LAST_DESIRED` => actual relay off, remembered desired preserved, note explains policy
- Recommended recovery-reset behavior:
  - always force actual relay off until a fresh command arrives
  - do not overwrite remembered desired when the force-off is a safety override
- Apply this policy after persistence is loaded and the prior reset cause is known, but before the panel starts.

### Route all execution through an action dispatcher, not the panel

- Add `app/src/actions/actions.c` and expand `actions.h` into a real dispatcher contract.
- The dispatcher should:
  - resolve `action_id`
  - reject missing or disabled actions
  - invoke the relay service
  - persist updated desired relay state when a command is accepted
  - record caller source for later status rendering
- Keep the public execution surface generic, for example `action_dispatcher_execute(action_id, source, result)`, so future scheduler work can call the same path without panel-specific logic.

### Seed reserved relay actions for immediate clean-device usability

- Materialize two reserved built-in actions during dispatcher startup if they are missing:
  - one action that drives relay 0 off
  - one action that drives relay 0 on
- Use stable reserved IDs and the existing persistence save path so the catalog stays durable and schedule-ready.
- Do not clobber non-empty user-owned entries; only fill the missing built-ins or fail closed if the catalog cannot safely accommodate them.

### Keep the HTTP and API surface thin and exact-path

- Preserve the Phase 5 cookie, session, and auth model.
- Add one relay-centric authenticated command route instead of separate on and off buttons or action-engine verbs.
- Keep the aggregate `GET /api/status` shape and make relay fields additive so the current frontend structure can grow without breaking unrelated cards.
- Keep HTTP handlers free of GPIO or persistence business logic; they should parse, authorize, delegate, and format response only.

### Replace the placeholder card with a single source-of-truth toggle

- Turn the relay card into a dedicated toggle surface with:
  - current actual state
  - remembered desired state
  - source badge
  - inline safety or recovery note
  - inline pending or failure reason
- Lock the control while a command is in flight or blocked.
- After a command, refresh the dashboard from `/api/status` rather than trusting optimistic browser state.

### Keep the first hardware binding intentionally narrow

- Add the first relay binding through board-owned devicetree or overlay configuration for the current `nrf7002dk/nrf5340/cpuapp` target.
- Enable only the minimal Zephyr GPIO plumbing needed for that driver.
- Do not pull multi-relay abstractions, schedule editors, or a reboot-policy editor into Phase 6.

</implementation_shape>

<validation>
## Validation Architecture

### Automated validation should remain build-first

- Keep `./scripts/build.sh` as the per-task automated signal for firmware changes.
- Keep `./scripts/validate.sh` as the canonical repo-owned wave and phase automated entrypoint.
- Preserve the fast shell-static gate already used in Phase 5:
  - `bash -n scripts/validate.sh scripts/common.sh scripts/build.sh`
- Add plan-specific structural `rg` checks for:
  - relay build and overlay wiring
  - the action-dispatcher entrypoint
  - the relay-centric HTTP control route
  - live relay status fields
  - negative checks that HTTP does not manipulate hardware directly and that Phase 6 still does not add scheduling, update, or config mutation routes

### A narrow deterministic seam is worth adding if execution can do it cheaply

- The repo still lacks a first-party test framework, but Phase 6 is the first place where a very small deterministic action-dispatch or relay-policy test could provide real value.
- If execution can add a minimal Zephyr-native test seam without derailing scope, the best target is the action and relay policy layer rather than the browser UI.
- If not, Phase 6 can still pass with the established build-first pattern plus strong manual sign-off, but ACT-02 will remain more architecture-proven than test-proven.

### Blocking manual sign-off should prove both control and safety

- Minimum real-device, browser, and curl scenarios:
  1. authenticated relay on from the browser toggle
  2. authenticated relay off from the browser toggle
  3. equivalent curl on and off commands against the new relay route
  4. local relay control still works when the device is reachable on the local LAN but upstream reachability is degraded
  5. ordinary reboot obeys the configured normal-boot policy
  6. recovery-triggered reboot forces the relay off until a fresh command is issued
  7. panel clearly shows actual state versus remembered desired state when safety override keeps them different
  8. toggle locks and explains pending, blocked, or failure cases inline
- Record scenario-level outcomes in the final summary rather than declaring the phase complete on build success alone.

### Validation and docs surfaces should follow the Phase 4 and Phase 5 pattern

- Extend `scripts/common.sh` with Phase 6 helper printers:
  - ready-state markers
  - relay curl commands
  - scenario labels
  - device and browser checklist
- Update `scripts/validate.sh` to keep automated validation and blocking manual sign-off clearly separated.
- Update `README.md` so the repo explains how to reproduce the relay control and safety validation flow on a clean checkout.

</validation>

<planning_implications>
## Planning Implications

- Preserve the roadmap split and keep dependencies strict:
  - `06-01` implements the relay driver abstraction, startup policy, and boot and runtime ownership
  - `06-02` implements the generic action model and execution pipeline
  - `06-03` wires panel and API commands through the dispatcher and completes end-to-end validation and docs
- Recommended dependency order:
  - `06-02` depends on `06-01`
  - `06-03` depends on `06-01` and `06-02`
- Keep these out of Phase 6:
  - schedule authoring or editing UX
  - multi-relay or multi-channel support
  - a general action-management UI
  - an operator-facing reboot-policy editor
  - OTA or update workflows
- The only material repo-level unknown is the exact first-relay GPIO binding and polarity for the current hardware target. The plan should isolate that to one overlay and build task so unresolved hardware mapping blocks a narrow surface instead of the whole phase.

</planning_implications>
