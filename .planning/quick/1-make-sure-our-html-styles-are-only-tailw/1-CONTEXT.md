# Quick Task 1: Tailwind-only panel styling and Bun-minified embedded JS - Context

**Gathered:** 2026-03-10
**Status:** Ready for planning

<domain>
## Task Boundary

Replace the panel's old Tailwind CDN import and handwritten stylesheet with the Tailwind v4 browser runtime requested by the user, and ensure the JavaScript embedded into firmware is Bun-minified before Zephyr gzips it into the generated include.

</domain>

<decisions>
## Implementation Decisions

### Tailwind runtime
- Lock the panel shell to `<script src="https://cdn.jsdelivr.net/npm/@tailwindcss/browser@4"></script>`.

### Styling source
- Remove the plain `<style>` block and keep panel styling in a `type="text/tailwindcss"` block so the semantic classes rendered by `main.js` remain stable.

### Firmware asset pipeline
- Minify `app/src/panel/assets/main.js` with `bun build --target=browser --format=iife --minify` before `generate_inc_file_for_target(... --gzip)` embeds the asset.

### Claude's Discretion
- Preserve the existing semantic class names and panel layout to avoid rewriting the panel runtime and template strings unnecessarily.
- Fail the build configuration clearly if `bun` is unavailable, because the requested firmware pipeline now depends on it.

</decisions>

<specifics>
## Specific Ideas

- Keep the current relay, scheduler, and OTA surfaces intact while changing only the styling source and asset preparation path.
- Limit the build change to `app/CMakeLists.txt` so `panel_http.c` can keep consuming `index.html.gz.inc` and `main.js.gz.inc` unchanged.

</specifics>
