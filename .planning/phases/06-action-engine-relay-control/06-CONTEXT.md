# Phase 6: Action Engine & Relay Control - Context

**Gathered:** 2026-03-10
**Status:** Ready for planning

<domain>
## Phase Boundary

Establish the generic action execution path and ship the first safe relay on/off operation. This phase turns the existing authenticated relay placeholder in the local panel into a real control surface and introduces the first live relay runtime. It does not add schedule authoring, multi-relay management, OTA workflows, or a broader settings editor.

</domain>

<decisions>
## Implementation Decisions

### Safe boot and recovery posture
- Normal reboot behavior must remain configurable rather than hardcoded to one fixed posture.
- Recovery-triggered reboot or confirmed fault recovery is stricter than ordinary reboot and forces the relay off until a fresh command is issued.
- If safety policy forces the applied relay state to differ from the remembered desired state, the panel should show both values clearly.
- Local relay commands remain allowed when the device is reachable on the local LAN, even if upstream internet reachability is degraded.

### Panel control flow
- The existing relay dashboard card should become a single-toggle control rather than separate on/off buttons.
- Relay commands should be one tap with clear in-flight and result feedback; routine use should not require an extra confirmation dialog.
- The relay card should prominently show actual state, remembered desired state, and the relevant safety or recovery note together.
- While a command is pending or blocked, the toggle should lock and explain the pending or failure reason inline on the card.

### Action model framing
- The operator experience should feel like direct relay control; the generic action engine stays behind relay-centric language.
- A clean device should be able to control the first relay immediately in Phase 6 without a separate action-setup step.
- Operator-facing status should show what last drove the relay state, such as manual control, safety fallback, or future automation.
- Action IDs or engine terminology should stay internal in Phase 6 even though manual commands and future schedules share the same execution path.

### Claude's Discretion
- Exact wording for the toggle labels, inline copy, and source badges.
- Exact representation of the configurable normal-boot policy, as long as it remains configurable and recovery remains stricter.
- Exact source badge taxonomy and how much detail appears in normal versus override states.
- Exact panel refresh cadence and status payload shape needed to support the locked behaviors.

</decisions>

<specifics>
## Specific Ideas

- The relay surface should feel immediate and lightweight for a single local operator, not like a workflow-engine console.
- Safety overrides should be obvious rather than silent; when the device chooses safety over remembered intent, the panel should make that mismatch visible.
- Local control should keep working through upstream degradation because the operator contract is local-LAN usefulness, not internet dependence.
- Phase 6 should feel like “the relay is now real” inside the existing panel, not like a new action-management product.

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `app/src/persistence/persistence_types.h` already persists action IDs, enabled flags, remembered relay intent, and reboot policy.
- `app/src/persistence/persistence.h` already provides request-shaped save/load APIs that validate staged config before writing.
- `app/src/panel/panel_http.c` already exposes authenticated JSON routes and reusable auth, cookie, and response helpers that future relay control routes can follow.
- `app/src/panel/panel_status.c` and `app/src/panel/assets/main.js` already render a protected relay card with placeholder copy, configured action count, remembered desired state, and reboot policy.
- `app/src/actions/actions.h` and `app/src/relay/relay.h` already reserve subsystem boundaries for the future action dispatcher and relay service.

### Established Patterns
- The device stays local-LAN-only with RAM-only server sessions and reboot-invalidated auth; Phase 6 should extend that exact trust model rather than expand access scope.
- `app_context` remains the single owner of resolved persisted state and shared runtime state for subsystems and panel views.
- The codebase favors typed subsystem boundaries, explicit status contracts, and aggregate panel JSON over ad hoc globals or log scraping.
- The panel remains available when local LAN reachability is healthy even if upstream internet is degraded.

### Integration Points
- Phase 6 runtime should initialize action and relay services before or alongside the existing panel bootstrap so the panel surfaces live state instead of a placeholder.
- Panel control routes should layer on the current authenticated HTTP shell instead of bypassing it or inventing a second control transport.
- Panel status needs to grow from “persisted intent plus placeholder” into “actual applied state plus remembered desired state plus source plus safety note”.
- Future scheduling will reference actions by `action_id`, so manual panel commands must use the same underlying action path even if the operator never sees action terminology.

</code_context>

<deferred>
## Deferred Ideas

- A dedicated settings or editor surface for changing the configurable reboot policy is not part of this phase; Phase 6 only locks that the behavior remains configurable.
- Visible action catalogs, action naming management, and broader action administration stay out of scope for this phase.
- Multi-relay channels and per-channel action targeting belong to later hardware-expansion work.
- Schedule creation and editing UX plus OTA workflows remain Phase 7 and Phase 8 work.

</deferred>

---
*Phase: 06-action-engine-relay-control*
*Context gathered: 2026-03-10*
