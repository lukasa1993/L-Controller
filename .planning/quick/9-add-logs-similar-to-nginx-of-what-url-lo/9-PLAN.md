---
quick_task: "9"
type: quick-full
description: "add logs similar to nginx of what url loaded how much bytes and how much time it took"
files_modified:
  - app/src/panel/panel_http.c
  - .planning/STATE.md
files_added:
  - .planning/quick/9-add-logs-similar-to-nginx-of-what-url-lo/9-SUMMARY.md
  - .planning/quick/9-add-logs-similar-to-nginx-of-what-url-lo/9-VERIFICATION.md
must_haves:
  truths:
    - "Panel HTTP requests emit one firmware-side access log on successful completion that identifies the served route plus response status, response byte count, and elapsed time in milliseconds."
    - "Existing request-capacity and terminated-request diagnostics from quick task 8 remain in place; this task adds normal access-style visibility without logging request bodies, cookies, credentials, or firmware payload contents."
    - "If bundled panel HTML/CSS/JS routes need callback instrumentation to expose per-URL logs, the implementation may wrap those resources in shared dynamic handlers, but route paths, content types, gzip encoding, and response payload bytes must stay unchanged."
  artifacts:
    - path: "app/src/panel/panel_http.c"
      provides: "Shared access-log helper(s), route timing metadata, and consistent final-response logging for panel routes"
      contains: "panel_http_log_access|duration_ms|response_ctx->body_len|route="
    - path: "app/src/panel/panel_http.c"
      provides: "Bundled static panel routes served through a shared callback path when needed so page and asset URLs can emit the same access-log contract"
      contains: "HTTP_RESOURCE_TYPE_DYNAMIC|content_encoding = \"gzip\"|/panel.css|/main.js|/login"
    - path: ".planning/quick/9-add-logs-similar-to-nginx-of-what-url-lo/9-CONTEXT.md"
      provides: "Locked task boundary, logging safety rules, and the allowed static-resource wrapping fallback"
      contains: "Logging Contract|Surface Area|Data Safety"
  key_links:
    - from: "app/src/panel/panel_http.c"
      to: "app/src/panel/panel_http.c"
      via: "route-context acquisition must capture start time once and the shared response-writing path must emit the completion log with the matching route metadata"
      pattern: "panel_http_acquire_route_context_logged|panel_http_write_json_response|panel_http_write_const_json_response"
    - from: "app/src/panel/panel_http.c"
      to: "app/src/panel/panel_http.c"
      via: "static panel resource definitions and any shared asset callback must preserve the current route strings, gzip encoding, and payload byte counts while adding access logging"
      pattern: "panel_shell_index_resource_detail|panel_shell_login_resource_detail|panel_shell_panel_css_resource_detail|panel_shell_main_js_resource_detail|HTTP_RESOURCE_DEFINE"
    - from: "app/src/panel/panel_http.c"
      to: ".planning/quick/9-add-logs-similar-to-nginx-of-what-url-lo/9-CONTEXT.md"
      via: "the implementation must follow the context lock that access logs include only safe response metadata and retain prior termination diagnostics"
      pattern: "panel_http_log_request_termination|panel_http_log_route_capacity|LOG_"
---

# Quick Task 9 Plan

## Objective

Add firmware-side access logging for the panel HTTP service so device logs show which URL was served, how many response bytes were returned, and how long the request took, while preserving the current panel routes and response contracts.

## Tasks

### Task 1: Add shared access-log timing and final-response logging for callback-backed routes
- Files: `app/src/panel/panel_http.c`
- Action: Extend the shared route-context and response-writing helpers so callback-backed panel routes capture a request start timestamp once, then emit a consistent completion log on final response write with the route label, HTTP status, response byte count, and elapsed milliseconds. Keep the existing capacity and aborted-request diagnostics intact and avoid logging request bodies, cookies, credentials, or upload payload content.
- Verify:
  - `rg -n "panel_http_log_access|duration_ms|route=|status=|bytes=" app/src/panel/panel_http.c`
  - `sed -n '250,380p' app/src/panel/panel_http.c`
  - `sed -n '900,980p' app/src/panel/panel_http.c`
- Done when: shared helpers own the success-path logging contract, dynamic handlers continue to return the same bodies/status codes, and normal completed API/runtime-config requests leave one route-aware access log each.

### Task 2: Wrap bundled panel assets in a shared callback path if needed to expose per-URL access logs
- Files: `app/src/panel/panel_http.c`
- Action: If the existing Zephyr static-resource path cannot emit access logs per route, replace the bundled panel HTML/CSS/JS resource details with shared dynamic resource handlers that serve the same gzipped payloads and content types while logging the route, bytes, and duration through the same helper contract used by the API handlers.
- Verify:
  - `rg -n "HTTP_RESOURCE_TYPE_DYNAMIC|content_encoding = \"gzip\"|/actions|/overview|/login|/panel.css|/main.js" app/src/panel/panel_http.c`
  - `sed -n '395,860p' app/src/panel/panel_http.c`
  - `git diff -- app/src/panel/panel_http.c`
- Done when: bundled panel page/asset routes that the browser loads have a callback-backed path capable of emitting the same access log format without changing route strings, gzip headers, content types, or payload bytes.

### Task 3: Rebuild, inspect the scoped logging change, and record the quick-task outcome
- Files: `.planning/STATE.md`, `.planning/quick/9-add-logs-similar-to-nginx-of-what-url-lo/9-SUMMARY.md`, `.planning/quick/9-add-logs-similar-to-nginx-of-what-url-lo/9-VERIFICATION.md`
- Action: Run the firmware build and focused source inspection to confirm the access logs cover the intended panel URLs, preserve existing termination diagnostics, and leave browser-facing contracts unchanged, then capture the scoped outcome in the standard quick-task summary, verification, and state files.
- Verify:
  - `./scripts/build.sh`
  - `rg -n "panel_http_log_access|panel_http_log_request_termination|panel_http_log_route_capacity|/panel.css|/main.js|/api/status|/api/actions|/api/update" app/src/panel/panel_http.c`
  - `git diff --stat`
- Done when: the firmware still builds, the modified panel routes show a coherent access-log path for URL/bytes/duration, and the quick-task artifacts record the completed change without widening scope beyond firmware-side logging.
