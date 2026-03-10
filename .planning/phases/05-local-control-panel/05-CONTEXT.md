# Phase 5: Local Control Panel - Context

**Gathered:** 2026-03-10
**Status:** Ready for planning

<domain>
## Phase Boundary

Deliver the local authenticated HTTP panel using authored HTML/JS assets and protected operator endpoints. This phase establishes the login/session flow plus the first operator status experience for device, Wi-Fi, relay, scheduler, and update state. It does not add relay-control actions, schedule editing UX, OTA mutation workflows, or a broader settings surface.

</domain>

<decisions>
## Implementation Decisions

### Session and access boundary
- Successful login should remain active until the operator explicitly logs out.
- Normal navigation and refresh during an active session should keep the operator signed in so browser session behavior feels predictable.
- Device reboot should force re-authentication; the browser should not silently resume a prior authenticated session after the firmware restarts.
- The single operator account may be used from multiple browsers or devices concurrently; a new login should not evict existing active sessions.
- Repeated wrong-password attempts should trigger a short cooldown rather than a permanent lockout or a fully frictionless retry loop.
- Session trust should stay on the firmware side with opaque RAM-only `sid` cookies; sessions are invalidated on reboot and stale cookies should be cleared cleanly.
- Phase 5 exposes only the auth trio plus one protected aggregate status surface: login, logout, session check, and `/api/status`.

### Dashboard hierarchy
- The post-login home should be a health overview first, not an equal-weight wall of domain cards.
- Connectivity plus overall device state should be the lead signal at the top of the home view.
- The home should stay glanceable: a few key values plus one short explanatory line per area, not a dense technical sheet.
- The information order on the home view should be health first, then operations, then maintenance.

### Degraded-state communication
- LAN-up but upstream-degraded status should read as caution, not alarm; the operator should understand that local control is still available.
- Recent recovery-reset or fault breadcrumbs should appear in the top summary and also have fuller detail in the recovery view.
- States that need attention, such as untrusted scheduler time or a staged update waiting for action, should remain visibly flagged until cleared without blocking the rest of the UI.
- Warning copy should stay terse and operational rather than soft or overly explanatory.

### Page structure for deferred domains
- Relay, scheduler, and update should be visible in Phase 5, but on their own pages rather than as main dashboard-home blocks.
- The dashboard home should stay focused on device-health orientation and use navigation links, not fake controls or heavy read-only cards, to lead into those domain pages.
- Dedicated relay, scheduler, and update pages should show live status plus a clear sense of current availability, but they should not invent inactive mock controls.
- The UI should not lean on roadmap or milestone language; it should speak in terms of what this build can currently do.

### Refresh behavior
- The panel should use quiet background refresh plus an explicit manual refresh control.
- Ordinary refreshes should happen silently; only meaningful state changes should visibly call for attention.
- The UI should show freshness metadata such as a last-updated timestamp and an obvious stale warning if refresh fails.
- Moving between the dashboard home and dedicated pages should reload fresh server truth on page entry rather than trusting long-lived browser state.

### Claude's Discretion
- Exact visual layout, typography, spacing, and card styling as long as the home remains health-first and glanceable.
- Exact navigation treatment for the dedicated relay, scheduler, and update pages.
- Exact cooldown threshold, duration, and failure wording.
- Exact copy on login, session-expired, and stale-data states as long as the tone stays terse and operator-oriented.
- Exact polling cadence and the exact threshold for when a background change becomes important enough to highlight.

</decisions>

<specifics>
## Specific Ideas

- The panel auth should feel simple and predictable for a single embedded operator rather than enterprise-style.
- "Manual logout only" is the desired steady-state posture during active use, but reboot is treated as a hard trust boundary.
- Concurrent browser access is acceptable even with a single local account; this phase should not invent takeover semantics unless a later need appears.
- A brief anti-guessing cooldown is preferred over a fully frictionless retry loop.
- The home view should orient the operator quickly around current health, while relay, scheduler, and update live on dedicated pages reachable from navigation.
- Only real, truthful availability should be shown. Avoid disabled mock controls, roadmap teasers, or UI hints that imply actions exist before they do.
- Local administration should still feel usable when upstream internet is degraded, with warnings that are clear but not panic-inducing.

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `app/src/panel/panel_http.c` and `app/src/panel/panel_http.h`: the canonical local admin server boundary. This is where auth, status, and later page-specific surfaces plug in.
- `app/src/panel/panel_auth.c` and `app/src/panel/panel_auth.h`: the single-user auth service with RAM-only session state and cooldown handling.
- `app/src/panel/panel_status.c` and `app/src/panel/panel_status.h`: the aggregate status builder that already translates device, network, recovery, relay, scheduler, and update state into operator-facing payloads.
- `app/src/panel/assets/index.html` and `app/src/panel/assets/main.js`: the authored embedded frontend shell; this is the home for dashboard/page hierarchy and refresh behavior decisions.
- `app/src/app/app_context.h` and `app/src/app/bootstrap.c`: the shared runtime seam that gives the panel one source of truth for persistence, network, recovery, relay, scheduler, and OTA state.
- `app/src/persistence/persistence.h` and `app/src/persistence/persistence_types.h`: the typed persisted auth and configuration contracts that the panel already depends on.

### Established Patterns
- The panel is local-LAN-only and single-operator by design; Phase 5 should not expand into multi-user, remote internet, or runtime Wi-Fi onboarding concerns.
- Frontend assets are authored HTML plus plain JavaScript embedded into firmware at build time, not C-generated markup and not a separate frontend toolchain.
- Browser state is intentionally thin: session truth comes from the server, and page data should be rehydrated from server responses rather than cached as the source of truth.
- Status presentation is built around one aggregate operator-facing payload instead of parallel per-domain status APIs for the home experience.
- The codebase prefers exact-route APIs, typed subsystem boundaries, and explicit operator-readable states over hidden globals or log scraping.
- The panel must remain usable when connectivity is locally healthy but upstream-degraded; this is already part of the broader network contract.

### Integration Points
- Login validation should consume the persisted auth snapshot already loaded into `app_context.persisted_config.auth`.
- Session-gated page loads and refreshes should rehydrate from the existing auth/session and aggregate status surfaces rather than inventing browser-held truth.
- The home dashboard and the dedicated relay/scheduler/update pages should all hang off the same embedded panel shell and navigation structure.
- Dedicated relay, scheduler, and update pages in Phase 5 should still be status-oriented; action routes for those domains belong to later phases.
- Later phase work extends the same `panel_http` surface with exact routes for relay, schedules, and OTA, so Phase 5 should establish page and navigation contracts that those later additions can reuse cleanly.

</code_context>

<deferred>
## Deferred Ideas

- Relay activation and deactivation controls belong to Phase 6.
- Schedule creation, editing, enable/disable, and deletion flows belong to Phase 7.
- OTA upload, remote-check, apply, and clear workflows belong to Phase 8.
- Auth settings beyond the single-user login/logout/session flow remain out of scope for Phase 5.

</deferred>

---
*Phase: 05-local-control-panel*
*Context gathered: 2026-03-10*
