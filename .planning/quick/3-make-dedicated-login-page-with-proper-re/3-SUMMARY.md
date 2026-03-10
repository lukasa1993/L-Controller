---
quick_task: "3"
completed: 2026-03-10
commit: 86e469d
affects: [panel, auth, redirects, playwright]
provides:
  - Dedicated embedded `/login` page based on a Tailwind Plus split-screen HTML pattern
  - Shared client-side redirect handling between `/login` and `/`
  - Updated embedded asset pipeline and panel discovery markers for the split-route shell
  - Redirect-aware Playwright coverage for the login entry flow
verification: partial
---

# Quick Task 3 Summary

## Outcome

- Added `app/src/panel/assets/login.html` and served it from firmware at `/login`, using a Tailwind Plus split-screen sign-in layout adapted for the local panel's HTML-only flow.
- Removed the inline login shell from `/` so the root page is now dashboard-only and starts hidden until the browser confirms an authenticated session.
- Refactored `app/src/panel/assets/main.js` so login, logout, bootstrap, and expired-session flows all share page-aware redirect logic, flash messaging, and a sanitized local `next` target.
- Extended the asset pipeline in `app/CMakeLists.txt` and `app/src/panel/panel_http.c` so `login.html` is gzip-embedded alongside the existing dashboard shell and `main.js`.
- Updated `tests/helpers/panel-target.js` to keep `/` auto-discovery working after the login form moved off the dashboard HTML.
- Updated `tests/panel-login.spec.js` and `README.md` so the documented smoke now checks the dedicated `/login` route and the redirect back to `/`.

## Verification Highlights

- `node --check app/src/panel/assets/main.js`, `node --check tests/helpers/panel-target.js`, and `node --check tests/panel-login.spec.js` passed.
- `./scripts/build.sh` passed and generated the new embedded `login.html.gz.inc` asset.
- `npx playwright test --list` passed and registered the updated redirect-aware smoke.
- `./scripts/flash.sh` passed and flashed the attached board through J-Link serial `1050756792`.
- `LNH_PANEL_BASE_URL=auto node tests/helpers/panel-target.js` resolved the flashed panel at `http://192.168.19.86`.
- A live browser check with known-invalid credentials confirmed the flashed device redirects `/` to `/login`, issues `/api/auth/login`, and renders the expected auth-failure alert.
- A local mocked browser smoke using the real `index.html`, `login.html`, and `main.js` assets passed the full success path: `/` -> `/login` -> `/` with dashboard rendering.
- The live success-path Playwright smoke remained blocked because the flashed device returned `401 invalid-credentials` for the local overlay credentials, which indicates the board currently holds different persisted auth than `app/wifi.secrets.conf`.
