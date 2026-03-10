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
    - "The panel shell imports Tailwind via jsDelivr's `@tailwindcss/browser@4` runtime and there is no stylesheet block left in `index.html`."
    - "The firmware build Bun-minifies `main.js` before the generated `main.js.gz.inc` payload is produced."
    - "The panel shell and JS-rendered surfaces use Tailwind utility classes directly, with a hidden HTML safelist for utility tokens that only appear in JS templates."
  artifacts:
    - path: "app/src/panel/assets/index.html"
      provides: "Tailwind v4 browser import, utility-only shell markup, and the safelist for JS-rendered utility tokens"
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
      via: "utility classes emitted by the runtime are safelisted in the HTML shell so Tailwind's browser runtime compiles them before the panel injects them"
      pattern: "hidden|border-emerald-400|grid-cols"
---

# Quick Task 1 Plan

## Objective

Make the embedded panel use only Tailwind-driven styling with the requested Tailwind v4 browser import, and ensure the firmware consumes a Bun-minified JavaScript asset instead of the raw `main.js` source.

## Tasks

### Task 1: Move the panel shell to Tailwind v4 browser styling
- Files: `app/src/panel/assets/index.html`
- Action: Replace the old Tailwind CDN reference and stylesheet block with the required Tailwind v4 browser import and utility-only markup in the HTML shell.
- Verify: `rg -n "jsdelivr.net/npm/@tailwindcss/browser@4|<style|text/tailwindcss|cdn\\.tailwindcss" app/src/panel/assets/index.html`
- Done when: the old CDN is gone, the required script is present, and `index.html` has no stylesheet block.

### Task 2: Minify the embedded panel JavaScript with Bun before gzip generation
- Files: `app/CMakeLists.txt`
- Action: Add a Bun-backed custom command that builds `main.js` for the browser as an IIFE with `--minify`, then feed that generated file into `generate_inc_file_for_target(... --gzip)`.
- Verify: `rg -n "BUN_EXECUTABLE|--target=browser|--format=iife|--minify|main.min.js" app/CMakeLists.txt`
- Done when: CMake produces `main.js.gz.inc` from the generated Bun artifact instead of the raw source file.

### Task 3: Verify the asset pipeline and panel shell end to end
- Files: `app/CMakeLists.txt`, `app/src/panel/assets/index.html`
- Action: Run the repo build so CMake executes the Bun step, compare source and generated bundle sizes, and smoke-test the panel shell in a browser against a static file server, including a probe for a utility class that only appears through the JS safelist path.
- Verify:
  - `./scripts/build.sh`
  - `wc -c app/src/panel/assets/main.js build/nrf7002dk_nrf5340_cpuapp/app/zephyr/panel-assets/main.min.js`
  - Browser smoke on `http://127.0.0.1:8123/index.html`
- Done when: the build passes, the Bun step runs before gzip generation, the minified asset is smaller than the source file, the shell renders without Tailwind runtime/style errors, and a JS-only utility token resolves to real computed styles.
