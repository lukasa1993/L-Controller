---
quick_task: "8"
type: quick-full
description: "Add firmware-side diagnostics for terminated panel HTTP requests and request-capacity failures so unresolved browser requests leave actionable device logs."
files_modified:
  - app/src/panel/panel_http.c
  - .planning/STATE.md
files_added:
  - .planning/quick/8-http-request-doesn-t-seem-to-resolve-pro/8-SUMMARY.md
  - .planning/quick/8-http-request-doesn-t-seem-to-resolve-pro/8-VERIFICATION.md
must_haves:
  truths:
    - "Dynamic panel HTTP handlers emit route-aware firmware logs when requests abort or fail because no route slot is available, without turning normal in-progress chunk callbacks into warning noise."
    - "OTA upload termination logs include staging state plus byte-progress metadata such as received versus declared content length, without logging payload bodies, credentials, or cookies."
    - "This pass preserves existing panel routes, response contracts, and browser behavior; it only improves firmware observability around request termination and capacity failures."
  artifacts:
    - path: "app/src/panel/panel_http.c"
      provides: "Shared logging helpers and call sites for request-capacity and aborted-request exits across panel handlers"
      contains: "panel_http_log_route_capacity|panel_http_log_request_termination|panel_http_data_status_name"
    - path: "app/src/panel/panel_http.c"
      provides: "OTA upload failure diagnostics with route label, error reason, and byte counters for abort, empty-upload, length mismatch, write failure, and finish failure branches"
      contains: "panel_http_log_upload_termination|stage-write-failed|empty-upload|content-length-mismatch|ota_service_finish_staging"
    - path: ".planning/quick/8-http-request-doesn-t-seem-to-resolve-pro/8-CONTEXT.md"
      provides: "Locked task boundary and firmware-only logging decision that the implementation must honor"
      contains: "firmware-side logging|request-capacity|payload bodies"
  key_links:
    - from: "app/src/panel/panel_http.c"
      to: "app/src/panel/panel_http.c"
      via: "the shared route-slot allocator and each dynamic handler must use one consistent logging contract for request-capacity and termination exits"
      pattern: "panel_http_acquire_route_context|panel_http_log_route_capacity|panel_http_log_request_termination"
    - from: "app/src/panel/panel_http.c"
      to: "app/src/panel/panel_http.c"
      via: "OTA upload rejection and abort logs must stay aligned with the upload route context and staging lifecycle"
      pattern: "panel_update_upload_handler|panel_http_log_upload_termination|stage-write-failed|empty-upload|content-length-mismatch|ota_service_finish_staging"
    - from: "app/src/panel/panel_http.c"
      to: "app/src/panel/assets/main.js"
      via: "the firmware logging-only change must leave the browser-side route and response consumers untouched"
      pattern: "/api/status|/api/actions|/api/schedules|/api/update"
---

# Quick Task 8 Plan

## Objective

Make unresolved panel requests diagnosable from firmware logs by instrumenting the existing `panel_http.c` early-exit and cleanup paths, especially where the browser currently sees a stalled or terminated request with no device-side explanation.

## Tasks

### Task 1: Add shared route-aware logging for panel request termination and slot exhaustion
- Files: `app/src/panel/panel_http.c`
- Action: Introduce shared helper logic that logs the route, HTTP data status, and safe progress metadata when a dynamic panel handler sees `HTTP_SERVER_DATA_ABORTED` or cannot acquire a route slot. Apply that helper consistently across the runtime-config, auth, status, action, relay, schedule, and update handlers instead of leaving silent `return 0` or bare `request-capacity` responses, while keeping normal in-progress chunk callbacks unlogged.
- Verify:
  - `rg -n "panel_http_log_route_capacity|panel_http_log_request_termination|panel_http_data_status_name|LOG_" app/src/panel/panel_http.c`
  - `sed -n '1968,2245p' app/src/panel/panel_http.c`
  - `sed -n '2240,4188p' app/src/panel/panel_http.c`
- Done when: request-capacity and aborted-request branches in the panel handlers emit route-aware logs before cleanup, normal chunk-progress callbacks remain quiet, and the implementation avoids logging request bodies, cookies, or credentials.

### Task 2: Instrument OTA upload abort and staging-failure paths with byte-progress diagnostics
- Files: `app/src/panel/panel_http.c`
- Action: Extend `/api/update/upload` logging so OTA staging open/write/finish failures, aborted uploads, empty uploads, and content-length mismatches all emit explicit firmware diagnostics that include the route, staging state, error reason, and `bytes_received` versus declared `content_length` where available, while preserving the current response contract.
- Verify:
  - `rg -n "panel_http_log_upload_termination|panel_update_upload_handler|ota_service_abort_staging|ota_service_begin_staging|ota_service_finish_staging|stage-write-failed|empty-upload|content-length-mismatch|bytes_received|content_length" app/src/panel/panel_http.c`
  - `sed -n '3554,3765p' app/src/panel/panel_http.c`
- Done when: OTA upload cleanup no longer discards staging context silently, and every upload termination path leaves enough device-side detail to distinguish cancellation, incomplete transfer, slot-open failure, and stage-write/finish failure.

### Task 3: Run focused firmware verification and record the quick-task outcome
- Files: `.planning/STATE.md`, `.planning/quick/8-http-request-doesn-t-seem-to-resolve-pro/8-SUMMARY.md`, `.planning/quick/8-http-request-doesn-t-seem-to-resolve-pro/8-VERIFICATION.md`
- Action: Rebuild the firmware, review the modified handler branches to confirm the new logs cover request-capacity, aborted-request, and OTA failure paths without changing browser-facing routes or response contracts, then record the scoped outcome in the standard quick-task summary, verification, and state files.
- Verify:
  - `./scripts/build.sh`
  - `rg -n "panel_http_log_route_capacity|panel_http_log_request_termination|panel_http_log_upload_termination|stage-write-failed|empty-upload|content-length-mismatch|ota_service_abort_staging|ota_service_finish_staging" app/src/panel/panel_http.c`
  - `rg -n "const routes =|/api/status|/api/actions|/api/schedules|/api/update" app/src/panel/assets/main.js`
  - `git diff --stat`
- Done when: the firmware build still passes, the verification notes show coverage for the targeted termination/capacity branches, and `STATE.md` records the completed quick task without expanding scope into browser logging changes.
