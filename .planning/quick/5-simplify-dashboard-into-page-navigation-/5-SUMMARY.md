# Quick Task 5 Summary

**Completed:** 2026-03-10
**Implementation Commit:** `66de52f`

## What changed

- Replaced the long protected dashboard shell with an actions-first panel that keeps `Actions` at `/` and adds dedicated `Overview`, `Schedules`, and `Updates` destinations.
- Refactored [app/src/panel/assets/main.js](/Users/l/_DEV/LNH-Nordic/app/src/panel/assets/main.js) so the active page stays stable during refreshes, silent polling no longer rewrites the whole shell, and relay actions show immediate optimistic feedback before live device truth settles.
- Added a dedicated output-topology rail beside the primary relay controls so the panel layout is ready for future multi-GPIO relay expansion without redesigning the shell again.
- Served the same embedded panel shell from [app/src/panel/panel_http.c](/Users/l/_DEV/LNH-Nordic/app/src/panel/panel_http.c) at `/actions`, `/overview`, `/schedules`, and `/updates`, and updated the Playwright discovery/smoke assets to match the new authenticated shell.

## Verification evidence

1. `node --check app/src/panel/assets/main.js`
   Result: passed.
   Notes: the route-aware shell, optimistic relay handling, and page navigation logic parse cleanly.

2. `node --check tests/helpers/panel-target.js`
   Result: passed.
   Notes: the discovery helper still parses after the protected shell markers changed.

3. `node --check tests/panel-login.spec.js`
   Result: passed.
   Notes: the updated smoke spec for the action-first shell and page navigation parses cleanly.

4. `npx playwright test tests/panel-login.spec.js --list`
   Result: passed.
   Notes: Playwright registered the updated authenticated-shell smoke test.

5. `./scripts/build.sh`
   Result: passed.
   Notes: the build regenerated `index.html.gz.inc`, `login.html.gz.inc`, and `main.js.gz.inc`, and the firmware embedded the new page routes successfully. Existing non-fatal MBEDTLS warnings remained, and the app image now uses 98.95% of the 32 KB flash region.

6. Live browser smoke against a real panel target
   Result: blocked.
   Notes: this session did not have `LNH_PANEL_*` or `CONFIG_APP_ADMIN_*` credentials configured, so the updated Playwright smoke could not be executed against a real device.

## Outcome

The local panel now prioritizes manual actions, keeps schedules and OTA on separate destinations, and no longer forces whole-shell redraws for routine refreshes. The remaining verification gap is live-device browser confirmation with real panel credentials.
