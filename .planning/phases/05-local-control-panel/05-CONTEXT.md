# Phase 5: Local Control Panel - Context

**Gathered:** 2026-03-09
**Status:** Ready for planning

<domain>
## Phase Boundary

Deliver the local authenticated HTTP panel using authored HTML/JS assets and protected operator endpoints. This phase establishes the login/session flow and the first read-only operator views for device, Wi-Fi, relay, scheduler, and update status. It does not add relay-control actions, schedule editing UX, or OTA workflows themselves.

</domain>

<decisions>
## Implementation Decisions

### Session flow
- Successful login should remain active until the operator explicitly logs out.
- Normal navigation and refresh during an active session should keep the operator signed in so browser session behavior feels predictable.
- Device reboot should force re-authentication; the browser should not silently resume a prior authenticated session after the firmware restarts.
- The single operator account may be used from multiple browsers or devices concurrently; a new login should not evict existing active sessions.
- Repeated wrong-password attempts should trigger a short cooldown rather than a permanent lockout or a fully frictionless retry loop.

### Claude's Discretion
- User chose to lock only the session-flow gray area during discussion; access-boundary details, dashboard hierarchy, and future-feature status presentation can stay flexible during research and planning.
- Exact login-screen structure, post-login landing route, and copywriting.
- Exact cooldown threshold, duration, and failure messaging.
- Exact session transport and storage mechanism, as long as the browser behavior above is preserved.

</decisions>

<specifics>
## Specific Ideas

- The panel auth should feel simple and predictable for a single embedded operator rather than enterprise-style.
- “Manual logout only” is the desired steady-state posture during active use, but reboot is treated as a hard trust boundary.
- Concurrent browser access is acceptable even with a single local account; this phase should not invent takeover semantics unless a later need appears.
- A brief anti-guessing cooldown is preferred over a fully frictionless retry loop.

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `app/src/panel/panel_http.h` already reserves the HTTP panel server boundary for Phase 5 work.
- `app/src/panel/panel_auth.h` already reserves the panel authentication service boundary and keeps auth separate from HTTP shell concerns.
- `app/src/app/app_context.h` already shares `persisted_config`, `network_state`, and `recovery`, giving panel and auth code a single runtime source of truth.
- `app/src/persistence/persistence.h` and `app/src/persistence/persistence_types.h` already provide typed auth persistence APIs and the single-user credential record.
- `app/src/network/network_state.h` and `app/src/recovery/recovery.h` already expose operator-meaningful connectivity and reset-status contracts for the dashboard views.

### Established Patterns
- Prior phases locked v1 to a local-LAN-only HTTP panel with one local operator account, so Phase 5 should not expand into multi-user or public-internet concerns.
- Persisted auth reseeds from build-time configured credentials when missing or corrupt, so Phase 5 can build on an already-defined credential source instead of inventing onboarding.
- The codebase prefers typed subsystem boundaries, explicit status enums, and operator-facing state contracts over hidden globals or log scraping.
- UI markup is expected to live as authored HTML/JS assets rather than C-generated strings.

### Integration Points
- Login validation should consume the persisted auth snapshot already loaded into `app_context.persisted_config.auth`.
- Session-gated routes will sit behind the future HTTP shell while protected views read `app_context.network_state`, `app_context.recovery`, and persisted status snapshots.
- Phase 5 must add the missing HTTP server and static-asset wiring to the build and bootstrap paths without disturbing the established boot and readiness flow.
- Later Phases 6-8 will hang control, scheduling, and update behavior off the panel shell created here, so the navigation and auth contracts established now become the base for those features.

</code_context>

<deferred>
## Deferred Ideas

- Relay activation and deactivation controls belong to Phase 6.
- Schedule creation and editing UX belong to Phase 7.
- OTA upload and remote-pull workflows belong to Phase 8.
- Within Phase 5 itself, access-boundary nuances, dashboard information hierarchy, and pre-feature placeholder presentation remain open for research and planning because the user chose to resolve only the session-flow gray area today.

</deferred>

---
*Phase: 05-local-control-panel*
*Context gathered: 2026-03-09*
