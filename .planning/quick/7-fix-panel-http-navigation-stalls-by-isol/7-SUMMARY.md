# Quick Task 7 Summary

**Completed:** 2026-03-11

## What changed

- Added `CONFIG_APP_PANEL_REQUEST_TIMEOUT_SECONDS` as the panel-owned timeout knob and made Zephyr's HTTP client inactivity timeout follow it through tracked Kconfig defaults.
- Refactored the panel HTTP handler state so dynamic routes no longer reuse one mutable singleton buffer per endpoint; each active HTTP client now gets its own route-context slot for login, status, action, schedule, relay, and OTA request handling.
- Added `/panel-config.js`, which publishes the effective request timeout from firmware to the browser runtime.
- Updated the page shell to load that runtime config before `main.js`, and taught JSON fetches in the panel runtime to fail cleanly with a timeout error instead of waiting forever during navigation/bootstrap.
- Logged the configured panel timeout and route-pool size when the panel HTTP server starts so the firmware's live configuration is visible on device.

## Outcome

Tracked code now bounds panel navigation failures to the configured timeout window and removes shared mutable handler buffers as a source of cross-request corruption. The remaining limitation is Zephyr's native dynamic-resource model itself: same-endpoint access is still serialized by the underlying server, so this is a practical hardening step rather than a full panel-server replacement.
