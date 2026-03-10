---
quick_task: "2"
completed: 2026-03-10
commit: 25cb217
affects: [panel, playwright, validation]
provides:
  - Real-device Playwright login smoke
  - Auto-discovery helper for the flashed panel URL
  - README instructions for the build, flash, and smoke workflow
  - Firmware fixes required to let the authenticated panel flow complete on device
---

# Quick Task 2 Summary

## Outcome

- Added a minimal Playwright runner at the repo root with Chromium-only configuration for a real-device panel smoke.
- Added `tests/helpers/panel-target.js`, which resolves the panel URL from either an explicit `LNH_PANEL_BASE_URL` or auto-discovery on the local subnet and requires operator credentials from environment variables.
- Added `tests/panel-login.spec.js`, which submits the real login form and only passes once the authenticated dashboard replaces the login shell and protected dashboard markers render.
- Documented the build, flash, discovery, and Playwright sequence in `README.md`.
- Moved the large `app_context` instance out of the `main()` stack after confirming it was `12648` bytes versus a `6144` byte main stack.
- Increased `CONFIG_HTTP_SERVER_STACK_SIZE` to `12288` so authenticated status rendering no longer overflows the HTTP worker thread.
- Fixed `panel_status_render_schedule_snapshot_json()` to return the rendered byte count instead of `0`, which previously made `/api/schedules` send `200` with an empty body.

## Verification Highlights

- `node --check tests/helpers/panel-target.js`, `node --check tests/panel-login.spec.js`, and `node --check playwright.config.js` passed.
- `npx playwright test --list` passed and registered the single Chromium smoke test.
- `./scripts/build.sh` passed after both the Playwright work and the follow-up firmware fixes.
- `./scripts/flash.sh` passed and flashed the attached board using J-Link serial `1050756792`.
- Live discovery resolved the flashed panel at `http://192.168.19.86`.
- `curl` confirmed successful authenticated responses from `/api/auth/login`, `/api/status`, `/api/update`, and `/api/schedules`.
- `LNH_PANEL_BASE_URL=http://192.168.19.86 npx playwright test tests/panel-login.spec.js` passed on the real device.

## Commit

- Code changes:
  - `570ae58` - `test(panel): add real-device login smoke`
  - `25cb217` - `fix(panel): unblock real-device login verification`
