---
quick_task: "2"
completed: 2026-03-10
commit: 570ae58
affects: [panel, playwright, validation]
provides:
  - Real-device Playwright login smoke
  - Auto-discovery helper for the flashed panel URL
  - README instructions for the build, flash, and smoke workflow
---

# Quick Task 2 Summary

## Outcome

- Added a minimal Playwright runner at the repo root with Chromium-only configuration for a real-device panel smoke.
- Added `tests/helpers/panel-target.js`, which resolves the panel URL from either an explicit `LNH_PANEL_BASE_URL` or auto-discovery on the local subnet and requires operator credentials from environment variables.
- Added `tests/panel-login.spec.js`, which submits the real login form and only passes once the authenticated dashboard replaces the login shell and protected dashboard markers render.
- Documented the build, flash, discovery, and Playwright sequence in `README.md`.

## Verification Highlights

- `node --check tests/helpers/panel-target.js`, `node --check tests/panel-login.spec.js`, and `node --check playwright.config.js` passed.
- `npx playwright test --list` passed and registered the single Chromium smoke test.
- `./scripts/build.sh` passed after the Playwright changes, and the build still generated the embedded panel assets successfully.
- `./scripts/flash.sh` passed and flashed the attached board using J-Link serial `1050756792`.
- The live panel never became discoverable on the host LAN afterward, so the actual browser smoke could not be executed against the flashed device during this run.

## Commit

- Code changes:
  - `570ae58` - `test(panel): add real-device login smoke`
