# Quick Task 9: add logs similar to nginx of what url loaded how much bytes and how much time it took - Context

**Gathered:** 2026-03-11
**Status:** Ready for planning

<domain>
## Task Boundary

Add firmware-side access logging for panel HTTP requests so device logs show which panel URL was served, how many bytes were returned, and how long the request took. Keep this focused on the existing panel HTTP service in `app/src/panel/panel_http.c`; do not broaden it into browser-console logging, transport redesign, or unrelated panel behavior changes.

</domain>

<decisions>
## Implementation Decisions

### Logging Contract
- Emit one completion log per successful panel response with the route, response status, response byte count, and elapsed time in milliseconds.
- Keep the existing terminated-request diagnostics from quick task 8; this task adds normal access-style visibility and should not remove abort/capacity logs.

### Surface Area
- Cover callback-backed API routes and runtime-config responses at minimum.
- If per-URL visibility for bundled HTML/CSS/JS requires wrapping Zephyr static resources in a shared dynamic handler, that refactor is allowed as long as routes, headers, encodings, and payload bytes stay unchanged.

### Data Safety
- Log response metadata only; do not log request bodies, cookies, credentials, firmware payload contents, or other sensitive data.
- Prefer one shared helper so access logs stay consistent across routes instead of ad hoc per-handler strings.

</decisions>

<specifics>
## Specific Ideas

- Reuse the existing route-context acquisition path to capture request start time and log on final response write.
- Use the known route string at each resource definition because Zephyr's `http_request_ctx` does not expose the URL in callbacks.
- Verify with a firmware build plus source inspection that the change preserves current routes and response contracts while adding route/bytes/duration logs.

</specifics>
