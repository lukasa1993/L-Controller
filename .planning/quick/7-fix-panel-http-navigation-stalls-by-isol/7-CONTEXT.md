# Quick Task 7: Fix panel HTTP navigation stalls by isolating requests and adding a configurable 10s timeout - Context

**Gathered:** 2026-03-11
**Status:** Ready for planning

<domain>
## Task Boundary

When the operator navigates between embedded panel pages, panel HTTP requests must stop sharing mutable singleton handler state and must not be able to hang indefinitely. The tracked fix needs to stay inside this repository, keep the current panel/API surface intact, and make the default request timeout 10 seconds while allowing builds to configure it.

</domain>

<decisions>
## Implementation Decisions

### Request Isolation
- Keep the fix in tracked application files instead of patching the untracked local `.ncs` tree.
- Replace singleton mutable route contexts in `app/src/panel/panel_http.c` with per-client route slots so each active client gets isolated request/response buffers for the panel handlers that currently reuse global state.

### Timeout Ownership
- Add a panel-owned timeout setting in app Kconfig with a default of 10 seconds.
- Use that setting to drive Zephyr HTTP client inactivity timeout through tracked Kconfig defaults instead of relying on the upstream library default implicitly.

### Surface Area
- Preserve the existing routes, JSON payloads, and page structure.
- Focus on server-side robustness first; only add client/runtime changes if they are required to make timeout behavior visible or verifiable.

</decisions>

<specifics>
## Specific Ideas

- Map route slot ownership by `struct http_client_ctx *` so chunked POST handlers can keep per-client accumulation state without sharing one static buffer.
- Keep the existing response helpers and status codes, but add a clear server-start log line that reports the configured panel request timeout.
- Verify with syntax/build checks that touch the modified firmware and panel runtime files, plus the tracked quick-task docs/state update flow.

</specifics>
