# Phase 9: Configured Action Model and Panel Management - Research

**Researched:** 2026-03-11
**Status:** Ready for planning

<focus>
## Research Goal

Define the smallest repo-aligned implementation shape that replaces boot-seeded built-in relay actions with an operator-managed configured action catalog without breaking persistence guarantees, authenticated panel patterns, or honest UI truth.

Phase 9 should create command-shaped configured relay actions, approved-output binding validation, and an authenticated CRUD surface on the Actions page. It should not yet make configured actions manually executable from the Actions page, selectable in schedule create/edit flows, or safe to delete while preserving schedule integrity; those integrations stay in Phase 10.

</focus>

<findings>
## Key Findings

### The current action model is still built around hidden built-in relay actions

- `app/src/actions/actions.c` defines `relay0.off` and `relay0.on`, seeds them during `action_dispatcher_init()`, and maps the public UI keys `relay-off` and `relay-on` back to those built-ins.
- `app/src/panel/panel_http.c` still treats manual relay control as a boolean request to `/api/relay/desired-state`, then resolves that boolean through `action_dispatcher_builtin_relay_action_id(...)`.
- Planning implication: Phase 9 must remove unconditional built-in seeding and stop treating boolean relay toggles as the operator-facing action model. The Actions page needs a real catalog and CRUD API, not another layer on top of hidden built-ins.

### The persisted action schema is too small for Phase 9 requirements

- `app/src/persistence/persistence_types.h` defines each action as only `action_id`, `enabled`, and `relay_on`.
- `app/src/persistence/persistence.c` validates only non-empty unique IDs; there is no display name, approved-output binding, action kind, command meaning beyond the relay boolean, or usability state.
- Planning implication: Phase 9 needs a richer configured-action record plus catalog helpers for ID generation, collision handling, approved-output validation, and operator-facing status derivation. The save path should stay inside the existing atomic sectioned persistence contract.

### Migration cannot rely on the current global layout-version reset behavior

- `app/src/persistence/persistence.c` treats non-auth schema mismatches as `reset-to-defaults`, and `persistence_store_load_actions()` resets the whole actions section on incompatibility.
- The current persistence layout version is shared across auth, actions, relay, schedules, and OTA, so a blunt layout-version bump for Phase 9 would also reset unrelated sections such as schedules.
- Planning implication: Phase 9 needs an explicit legacy-action migration or normalization path for the action section rather than a global incompatible reset. Legacy built-in action records such as `relay0.on` and `relay0.off` must be handled deliberately, and boot must never reintroduce hidden built-in execution as a fallback.

### Legacy schedule references make the cutover boot-critical even though schedule UX is deferred

- `app/src/persistence/persistence.c` validates the saved schedule table against the loaded action catalog during both load and save, and it resets the schedule section if validation fails.
- Existing schedules persist canonical `action_id` values, so deleting `relay0.on` and `relay0.off` from the effective action catalog too early can invalidate saved schedules during boot.
- Planning implication: Phase 9 must preserve legacy schedule references as compatibility-only behavior so boot and save validation do not wipe the schedule table. Full schedule-reference migration, schedule picker adoption, and schedule integrity flows still belong to Phase 10.

### The relay boundary already owns the approved hardware truth, but not an operator-facing binding catalog

- `app/boards/nrf7002dk_nrf5340_cpuapp.overlay` declares `relay_outputs` with the `relay0` alias and a friendly devicetree `label = "Relay 0"`.
- `app/src/relay/relay.c` already owns hardware readiness and runtime state, but it exposes only apply/status APIs today.
- Planning implication: Phase 9 can derive an approved-output registry from firmware-owned relay definitions instead of adding raw GPIO entry to the panel. That keeps binding selection explicit, safe, and future-extensible.

### The panel already has the right exact-route and authenticated mutation patterns

- `app/src/panel/panel_http.c` already provides authenticated exact-route handlers and the persistence-save-then-runtime-reload pattern used by schedules.
- `app/src/panel/assets/main.js` already moved the authenticated shell to an actions-first layout, but the Actions page is currently an honest placeholder that explicitly says output configuration is not implemented yet.
- Planning implication: Phase 9 should reuse the existing panel route style to add `/api/actions` snapshot and mutation endpoints, then replace the placeholder with a create-first management catalog.

### Schedule and status surfaces still hard-code relay public keys and must stay out of scope

- `app/src/panel/panel_status.c` hard-codes `actionChoices` to `relay-on` and `relay-off` whenever `outputsConfigured` becomes true.
- `app/src/panel/panel_http.c` schedule create and update handlers only accept action keys through `action_dispatcher_action_id_from_public_key(...)`.
- `app/src/scheduler/scheduler.c` already executes canonical `action_id` values and validates saved schedules against the persisted action catalog.
- Planning implication: Phase 9 should not switch the Schedules surface to the configured catalog yet. It should keep schedule-action adoption deferred to Phase 10 while preserving legacy built-in schedule references only as compatibility truth so boot does not invalidate existing schedules.

### The repo already has a validation stack Phase 9 can extend

- `./scripts/build.sh` remains the fastest automated repo signal and `./scripts/validate.sh` remains the canonical validation command.
- The repo also has a device-backed Playwright harness in `tests/panel-login.spec.js` and `tests/helpers/panel-target.js`.
- Planning implication: Phase 9 can stay build-first for automated feedback, then add browser or curl checks for the configured-action management surface without introducing a new framework.

</findings>

<implementation_shape>
## Recommended Plan Shape

### 09-01 should establish the configured action record, approved-output registry, explicit legacy normalization, and schedule compatibility guardrails

- Extend the persisted action contract from a built-in relay boolean into a configured-action record that can hold operator-facing name, generated stable ID, approved output binding, command meaning, enabled state, and future-extensible type metadata.
- Add firmware-owned helpers for approved output lookup and action usability evaluation so the panel never accepts free-form GPIO entry.
- Replace unconditional built-in seeding with an explicit legacy-action normalization path that drops or quarantines old built-in action records from the operator catalog without silently reintroducing them as the primary model.
- Add compatibility handling for legacy schedule references so schedule load or save validation does not reset saved schedules before Phase 10 owns the full schedule-reference migration.

### 09-02 should deliver authenticated configured-action snapshot, create, and edit APIs

- Add exact-route authenticated action handlers that expose catalog truth and accept operator inputs for create and edit flows.
- Generate operator-safe action IDs on the device from the friendly name, enforce uniqueness and approved-output validation server-side, and keep mutation errors field-specific and operator-readable.
- Reuse the existing atomic persistence save pattern so catalog updates land durably and runtime truth refreshes immediately after each accepted mutation.

### 09-03 should replace the placeholder Actions surface with a real management catalog while keeping execution deferred

- Replace the current placeholder or relay-toggle surface with a create-first action catalog that shows friendly name, output summary, enabled state, and usability badges such as `Ready`, `Disabled`, and `Needs attention`.
- Add create and edit affordances on the Actions page, but do not add execute buttons yet. Phase 10 will make configured actions executable once the shared execution and schedule-catalog work exists.
- Keep the Schedules surface honest and deferred. The new catalog should not leak into schedule create/edit choices until Phase 10 makes that path canonical.

</implementation_shape>

<validation>
## Validation Architecture

### Automated validation should stay build-first and structural

- Keep `./scripts/build.sh` as the per-task fast signal and `./scripts/validate.sh` as the canonical full automated command.
- Add structural verification that proves:
  - built-in relay action seeding is gone
  - configured-action records include name and approved-output binding fields
  - action CRUD routes exist on the authenticated panel API
  - the Actions page renders catalog management rather than direct relay execution
  - schedule action-choice wiring is still deferred instead of partially switched to an inconsistent contract

### Final approval still needs browser, curl, and device checkpoints

Minimum scenario set:

1. **Clean boot truth** — a clean device no longer depends on hidden built-in relay actions and the Actions page shows the configured-action empty state instead of fake-ready manual controls.
2. **Create valid configured action** — an authenticated operator creates a valid relay action, the device generates a stable operator-safe ID, the catalog shows the action as `Ready`, and the record survives reboot.
3. **Reject unsafe binding** — a forged or invalid API request with an unsupported output binding is rejected before the action becomes usable.
4. **Edit and disable** — an authenticated operator edits the display name, binding, and enabled state of an existing action and sees truthful `Ready` or `Disabled` status after save and refresh.
5. **Legacy built-in migration** — persisted legacy built-in action records are handled explicitly on boot or first load, and the firmware does not silently restore `relay0.on` or `relay0.off` as the primary operator action model.
6. **Legacy schedule preservation** — an existing schedule table that still references legacy built-in IDs survives Phase 9 boot and validation without being wiped, while those references remain explicit compatibility-only behavior.
7. **Schedule deferral remains honest** — after Phase 9 lands, the Schedules surface still does not pretend configured actions are selectable until the Phase 10 shared-catalog work is complete.

</validation>
