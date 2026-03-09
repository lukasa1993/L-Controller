# Phase 4: Persistent Configuration - Context

**Gathered:** 2026-03-09
**Status:** Ready for planning

<domain>
## Phase Boundary

Create a validated persistent configuration layer for the single local operator account, action and relay definitions, relay reboot rules, and schedules. This phase defines what configuration exists, what survives reboot, and how invalid stored data falls back safely. It does not add multi-user auth, runtime Wi-Fi onboarding, open recovery access, or the operator-facing panel flows that will edit this data later.

</domain>

<decisions>
## Implementation Decisions

### Baseline configuration
- On a clean device, seed the single local operator credentials once from the configured `APP_ADMIN_USERNAME` / `APP_ADMIN_PASSWORD` source, then persist that account as the auth baseline.
- The product still supports exactly one local operator account.
- Both username and password should behave as fixed credentials, not casual runtime-edit fields; changing them requires a deliberate configuration change path rather than ordinary panel editing.
- Clean devices should start with no persisted actions or schedules.
- Initial relay-related defaults should be conservative and safe.
- Relay-related persistence must include the last desired state, not only static definitions.
- Reboot behavior for that last desired relay state must itself be configurable, rather than hard-coded to always restore or always ignore it.
- The clean-device default should remain the safe posture until a later configuration explicitly chooses otherwise.

### Corrupt or incompatible data fallback
- Corruption handling should isolate by section: a broken schedule or relay/action section should not wipe healthy auth or other valid persisted data.
- If the auth section is missing or invalid, recreate the fixed single operator credentials from the configured source so the operator retains a known recovery path.
- Stored data from an incompatible version should reset safely rather than attempting best-effort migration or leaving ambiguous behavior.
- Config fallback events should be loud and specific: later status surfaces should identify which section failed and whether it was reset, disabled, or reseeded.

### Save and validation rules
- Persisted changes should normally save section by section rather than rewriting one giant whole-device snapshot for every edit.
- Future phases may store disabled but otherwise valid entries, especially for action and schedule records.
- Incomplete or malformed drafts should not be persisted.
- Each submitted save request should be atomic at the request level: if part of that request is invalid, reject the whole request instead of partially applying it.
- Persistence should only accept self-consistent references between saved items, such as schedules pointing at already valid action definitions.
- Validation should happen before data becomes durable, not deferred until runtime execution.

### Claude's Discretion
- Exact storage backend and supported write strategy.
- Exact record formats, version stamps, and per-section repository API shapes.
- Exact method for seeding or replacing configured auth values while preserving the fixed-credential posture chosen here.
- Exact log and status field names plus the operator-visible wording for reset, reseed, or validation outcomes.
- Exact representation of the configurable relay reboot policy, as long as the safe default and stored last-desired-state requirements above are preserved.

</decisions>

<specifics>
## Specific Ideas

- This phase should create durable contracts for later `panel_auth`, `actions`, `relay`, and `scheduler` work rather than sneaking in panel UX or execution behavior early.
- Clean-device behavior matters: the device should feel intentionally blank but safe, not preloaded with demo automation.
- The user explicitly prefers fixed single-operator credentials that are only changed through a deliberate config path.
- The user wants relay intent to survive as persisted data, but not with a one-size-fits-all reboot rule; that reboot rule must be configurable.
- Later editing flows should support disabled-but-valid records, but should never save dangling references or half-valid writes.

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `app/Kconfig` already defines `APP_ADMIN_USERNAME` and `APP_ADMIN_PASSWORD`, giving Phase 4 a concrete configured source for first-boot auth seeding.
- `app/src/app/bootstrap.c` already centralizes build-time config loading and subsystem startup, making it the natural seam for clean-boot seeding, persisted-config load, and loud fallback logging.
- `app/src/persistence/persistence.h` already reserves the persistence subsystem boundary for a real store implementation.
- `app/src/panel/panel_auth.h`, `app/src/actions/actions.h`, `app/src/relay/relay.h`, and `app/src/scheduler/scheduler.h` already reserve the downstream consumers that will read the Phase 4 contracts.
- `app/src/app/app_context.h` already acts as the shared runtime carrier and is the obvious place where future subsystems can receive resolved persisted configuration state.

### Established Patterns
- The codebase already prefers typed subsystem boundaries behind `main.c` and `app_boot()`, so persisted configuration should be exposed through explicit typed APIs rather than leaked globals.
- Earlier phases established named status, structured failure records, and loud logging; persistence fallback should match that operator-meaningful style.
- Phase 3 already treats the most recent recovery breadcrumb as a separate concern, so Phase 4 should stay focused on durable operator configuration rather than expanding into general incident history.
- Current project decisions keep Wi-Fi credentials build-time configured and limit auth to one local operator, which narrows Phase 4's scope and avoids runtime onboarding concerns.

### Integration Points
- Boot-time loading and seeding belongs on the `app_boot()` path after build-time config is available and before downstream subsystems need resolved persisted data.
- Auth reseeding must integrate with the configured admin credential source already exposed through Kconfig.
- Future panel/auth, relay/action, and scheduler phases need section-level persistence contracts that can distinguish valid-active, valid-disabled, reset-to-default, and invalid-or-reseeded states.
- Later status surfaces should be able to explain config fallback outcomes using the same structured, operator-facing approach already used for network and recovery status.

</code_context>

<deferred>
## Deferred Ideas

- Ordinary runtime UX for changing the fixed credentials belongs to later auth and panel work; Phase 4 only locks the persistence contract and deliberate config-change posture.
- The exact relay-output behavior on reboot after reading the configurable policy belongs to the relay-control phase; Phase 4 only locks that the policy and last desired state must both be persistable.
- Multi-user auth, open recovery access, config history browsers, and richer draft-edit workflows remain out of scope for this phase.

</deferred>

---

*Phase: 04-persistent-configuration*
*Context gathered: 2026-03-09*
