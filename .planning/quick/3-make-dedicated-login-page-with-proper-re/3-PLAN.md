---
quick_task: "3"
type: quick-full
description: "Make a dedicated login page with proper redirect-on-success using a Tailwind Plus paid HTML login flow."
files_modified:
  - app/CMakeLists.txt
  - app/src/panel/assets/index.html
  - app/src/panel/assets/login.html
  - app/src/panel/assets/main.js
  - app/src/panel/panel_http.c
  - tests/helpers/panel-target.js
  - tests/panel-login.spec.js
  - README.md
autonomous: true
must_haves:
  truths:
    - "The embedded panel serves a dedicated `/login` page and keeps `/` as the dashboard route."
    - "Unauthenticated dashboard visits redirect to `/login`, and successful login redirects back to a sanitized local return path."
    - "The Playwright smoke covers the redirect contract, not just the raw login POST."
  artifacts:
    - path: "app/src/panel/assets/login.html"
      provides: "Dedicated Tailwind Plus-inspired login entrypoint"
      contains: "data-panel-page=\"login\"|login-form|login-submit"
    - path: "app/src/panel/assets/main.js"
      provides: "Shared client-side session bootstrap and redirect handling"
      contains: "redirectToLogin|normalizeNextPath|bootstrapSession"
    - path: "app/src/panel/panel_http.c"
      provides: "Embedded `/login` route wiring"
      contains: "login.html.gz.inc|\"/login\""
  key_links:
    - from: "tests/panel-login.spec.js"
      to: "app/src/panel/assets/main.js"
      via: "the smoke should observe the same redirect contract implemented by the shared browser bootstrap"
      pattern: "/login|normalizeNextPath|redirectToLogin"
    - from: "tests/helpers/panel-target.js"
      to: "app/src/panel/assets/index.html"
      via: "auto-discovery probes `/` and must keep matching the dashboard shell after the login form is removed from that page"
      pattern: "data-panel-page=\"dashboard\"|dashboard-view|LNH Nordic Mission Console"
---

# Quick Task 3 Plan

## Objective

Split the local panel into a dedicated login page and a protected dashboard route while keeping the firmware auth APIs unchanged.

## Tasks

### Task 1: Add the dedicated embedded login asset and route
- Files: `app/CMakeLists.txt`, `app/src/panel/assets/login.html`, `app/src/panel/assets/index.html`, `app/src/panel/panel_http.c`
- Action: Add a new gzipped login asset, serve it at `/login`, adapt the Tailwind Plus split-screen sign-in pattern for the embedded HTML-only flow, and simplify the dashboard HTML so `/` no longer contains the login shell.
- Verify:
  - `rg -n "/login|login.html.gz.inc|data-panel-page" app/CMakeLists.txt app/src/panel/assets/login.html app/src/panel/assets/index.html app/src/panel/panel_http.c`
  - `./scripts/build.sh`
- Done when: the firmware serves two distinct HTML entrypoints and the build still produces embedded assets cleanly.

### Task 2: Refactor shared browser auth flow around page-aware redirects
- Files: `app/src/panel/assets/main.js`
- Action: Make the shared browser runtime page-aware, add safe `next` handling and redirect flash messages, redirect unauthenticated dashboard sessions to `/login`, and redirect successful login back to `/`.
- Verify:
  - `node --check app/src/panel/assets/main.js`
  - `./scripts/build.sh`
- Done when: login, logout, bootstrap, and expired-session cases all use the new dedicated route contract without breaking dashboard refreshes.

### Task 3: Update discovery, smoke coverage, and operator docs
- Files: `tests/helpers/panel-target.js`, `tests/panel-login.spec.js`, `README.md`
- Action: Update shell markers for auto-discovery, rewrite the Playwright smoke to cover `/` to `/login` to `/`, and document the dedicated login route expectation.
- Verify:
  - `node --check tests/helpers/panel-target.js`
  - `node --check tests/panel-login.spec.js`
  - `npx playwright test --list`
- Done when: the smoke and docs describe and validate the new redirect-driven login flow accurately.
