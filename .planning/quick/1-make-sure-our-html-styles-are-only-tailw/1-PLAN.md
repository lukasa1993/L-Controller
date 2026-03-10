---
quick_task: "1"
type: quick-full
description: "Move the panel to Tailwind v4 browser styling and embed a Bun-minified JavaScript asset into firmware."
files_modified:
  - app/CMakeLists.txt
  - app/src/panel/assets/index.html
autonomous: true
must_haves:
  truths:
    - "The panel shell imports Tailwind via jsDelivr's `@tailwindcss/browser@4` runtime and panel styling lives in a `text/tailwindcss` block instead of a handwritten stylesheet."
    - "The firmware build Bun-minifies `main.js` before the generated `main.js.gz.inc` payload is produced."
    - "The existing panel runtime keeps the same semantic class names so the control, scheduler, and OTA surfaces do not need a JS template rewrite."
  artifacts:
    - path: "app/src/panel/assets/index.html"
      provides: "Tailwind v4 browser import and Tailwind-defined component styles for the panel shell"
      contains: "@tailwindcss/browser@4"
    - path: "app/CMakeLists.txt"
      provides: "Bun custom command that generates the minified browser-safe panel bundle before gzip embedding"
      contains: "Minifying panel JavaScript with Bun"
  key_links:
    - from: "app/CMakeLists.txt"
      to: "app/src/panel/panel_http.c"
      via: "the generated `main.js.gz.inc` resource is now built from a Bun-minified bundle but consumed through the same HTTP resource include"
      pattern: "main.js.gz.inc|main.min.js"
    - from: "app/src/panel/assets/index.html"
      to: "app/src/panel/assets/main.js"
      via: "semantic classes emitted by the runtime stay styled through the Tailwind component layer in the HTML shell"
      pattern: "badge|card|scheduler|update"
---

# Quick Task 1 Plan

## Objective

Make the embedded panel use only Tailwind-driven styling with the requested Tailwind v4 browser import, and ensure the firmware consumes a Bun-minified JavaScript asset instead of the raw `main.js` source.

## Tasks

### Task 1: Move the panel shell to Tailwind v4 browser styling
- Files: `app/src/panel/assets/index.html`
- Action: Replace the old Tailwind CDN reference and handwritten stylesheet with the required Tailwind v4 browser import plus a `text/tailwindcss` component layer that keeps the existing semantic panel classes working.
- Verify: `rg -n "jsdelivr.net/npm/@tailwindcss/browser@4|text/tailwindcss|cdn\\.tailwindcss" app/src/panel/assets/index.html`
- Done when: the old CDN is gone, the required script is present, and the panel shell still renders with the existing semantic classes.

### Task 2: Minify the embedded panel JavaScript with Bun before gzip generation
- Files: `app/CMakeLists.txt`
- Action: Add a Bun-backed custom command that builds `main.js` for the browser as an IIFE with `--minify`, then feed that generated file into `generate_inc_file_for_target(... --gzip)`.
- Verify: `rg -n "BUN_EXECUTABLE|--target=browser|--format=iife|--minify|main.min.js" app/CMakeLists.txt`
- Done when: CMake produces `main.js.gz.inc` from the generated Bun artifact instead of the raw source file.

### Task 3: Verify the asset pipeline and panel shell end to end
- Files: `app/CMakeLists.txt`, `app/src/panel/assets/index.html`
- Action: Run the repo build so CMake executes the Bun step, compare source and generated bundle sizes, and smoke-test the panel shell in a browser against a static file server.
- Verify:
  - `./scripts/build.sh`
  - `wc -c app/src/panel/assets/main.js build/nrf7002dk_nrf5340_cpuapp/app/zephyr/panel-assets/main.min.js`
  - Browser smoke on `http://127.0.0.1:8123/index.html`
- Done when: the build passes, the Bun step runs before gzip generation, the minified asset is smaller than the source file, and the panel shell renders without Tailwind runtime/style errors.
