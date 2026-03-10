# Quick Task 1: Tailwind-only panel styling and Bun-minified embedded JS - Context

**Gathered:** 2026-03-10
**Status:** Ready for planning

<domain>
## Task Boundary

Replace the panel's old Tailwind CDN import and handwritten stylesheet with the Tailwind v4 browser runtime requested by the user, remove the custom stylesheet layer entirely, and ensure the JavaScript embedded into firmware is Bun-minified before Zephyr gzips it into the generated include.

</domain>

<decisions>
## Implementation Decisions

### Tailwind runtime
- Lock the panel shell to `<script src="https://cdn.jsdelivr.net/npm/@tailwindcss/browser@4"></script>`.

### Styling source
- Remove the stylesheet layer entirely and express panel styling through Tailwind utility classes in `index.html` and `main.js`.
- Add one hidden utility safelist element in `index.html` for Tailwind tokens that only appear inside JS-rendered template strings.

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
