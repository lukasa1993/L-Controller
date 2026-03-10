---
quick_task: "2"
type: quick-full
description: "Build, flash, and add a Playwright login smoke that verifies the first authenticated page is the dashboard."
files_modified:
  - package.json
  - package-lock.json
  - .gitignore
  - playwright.config.js
  - tests/panel-login.spec.js
  - tests/helpers/panel-target.js
  - README.md
autonomous: true
must_haves:
  truths:
    - "The repo contains a Playwright smoke test that submits the real panel login form and only passes once the authenticated dashboard replaces the login shell."
    - "The Playwright path can target a flashed device automatically on the local subnet without hard-coding the device URL into source control."
  artifacts:
    - path: "tests/panel-login.spec.js"
      provides: "The login smoke and post-login dashboard assertions"
      contains: "Authenticated as|Device shell|Connectivity"
    - path: "tests/helpers/panel-target.js"
      provides: "Target resolution for explicit URLs and auto-discovered device URLs"
      contains: "resolvePanelBaseUrl|auto-discover"
    - path: "playwright.config.js"
      provides: "Playwright runner configuration for the login smoke"
      contains: "playwright/test"
  key_links:
    - from: "tests/panel-login.spec.js"
      to: "app/src/panel/assets/main.js"
      via: "the smoke asserts the same login-to-dashboard transition and dashboard card titles driven by the embedded panel runtime"
      pattern: "showDashboardView|Authenticated as|Device shell|Connectivity"
    - from: "tests/helpers/panel-target.js"
      to: "app/Kconfig"
      via: "auto-discovery probes the configured panel HTTP port, which defaults to the firmware's APP_PANEL_PORT value"
      pattern: "CONFIG_APP_PANEL_PORT|default 80"
---

# Quick Task 2 Plan

## Objective

Add a Playwright-based login smoke that validates the real panel login flow after a manual firmware build and flash on the actual device.

## Tasks

### Task 1: Add the Playwright runner and target-resolution helpers
- Files: `package.json`, `package-lock.json`, `.gitignore`, `playwright.config.js`, `tests/helpers/panel-target.js`
- Action: Introduce the minimal Node and Playwright setup for this firmware repo, add ignore rules for Playwright artifacts, and implement helper logic that resolves the panel base URL from an explicit env var, a local mock URL, or auto-discovery across the host's active private `/24` subnet on the panel port.
- Verify:
  - `npm install`
  - `node --check tests/helpers/panel-target.js`
  - `npx playwright test --list`
- Done when: Playwright config loads cleanly, dependencies install, and the target helper can be parsed and referenced by the test runner.

### Task 2: Build the login smoke against the real panel shell
- Files: `tests/panel-login.spec.js`
- Action: Write a Playwright smoke that opens the panel, fills the login form, submits credentials, waits for the protected dashboard refresh to complete, and asserts that the login view is hidden while stable dashboard markers such as the authenticated session chip plus `Device shell` and `Connectivity` are visible.
- Verify:
  - `node --check tests/panel-login.spec.js`
  - `LNH_PANEL_BASE_URL=auto npx playwright test tests/panel-login.spec.js`
- Done when: the smoke passes against the flashed device through auto-discovery and its assertions align with the real firmware panel contracts.

### Task 3: Run and document the live hardware build, flash, and smoke sequence
- Files: `README.md`
- Action: Document how to install Playwright dependencies, build and flash firmware manually, provide credentials securely, and let the smoke auto-discover the flashed device. Execute the real-device build, flash, discovery, and Playwright smoke sequence when hardware is available; if the environment blocks that run, record the exact blocker instead of treating the path as verified.
- Verify:
  - `rg -n "Playwright|LNH_PANEL|auto-discover|build.sh|flash.sh" README.md`
  - `./scripts/build.sh`
  - `./scripts/flash.sh`
  - `node tests/helpers/panel-target.js`
  - `LNH_PANEL_BASE_URL=auto npx playwright test tests/panel-login.spec.js`
- Done when: README explains both the real-device path and the local mock validation path without adding a new wrapper workflow, and the live path is either exercised successfully or blocked with the precise missing prerequisite recorded.
