# Quick Task 8: http request doesn't seem to resolve properly in browser if we terminating them we need to write logs so we know whats going on - Context

**Gathered:** 2026-03-11
**Status:** Ready for planning

<domain>
## Task Boundary

When panel HTTP requests time out, abort, or otherwise fail to finish cleanly, the firmware needs to emit enough diagnostics to explain what happened. Keep this pass focused on device-side logging in the existing panel HTTP stack; do not broaden it into a transport redesign or browser-console instrumentation change.

</domain>

<decisions>
## Implementation Decisions

### Logging Scope
- Record diagnostics on the firmware side only.
- Do not add browser-side logging in this task unless it becomes strictly necessary to validate the firmware behavior.

### Logging Content
- Prefer structured route-aware logs that identify which handler saw the termination and whether it was an abort, an incomplete request, or another capacity/state failure.
- Log progress metadata such as accumulated/requested bytes when available, but do not log request payload bodies, credentials, cookies, or firmware binary contents.

### Surface Area
- Keep the current panel routes, response contracts, and timeout behavior intact.
- Focus on the existing request-status branches and cleanup paths in `app/src/panel/panel_http.c`, including OTA upload staging abort paths where termination currently discards state silently.

</decisions>

<specifics>
## Specific Ideas

- Add shared helper(s) so common termination cases are logged consistently instead of adding ad hoc `LOG_*` lines in every handler.
- Include capacity/slot-acquisition failures in the logging pass because they present as unresolved requests from the browser side.
- Verify by rebuilding the firmware and by reviewing the modified handler branches to confirm the new logs cover abort, non-final, and staging-abort paths.

</specifics>
