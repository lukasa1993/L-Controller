# Codebase Concerns

## Scope reviewed
- Reviewed the tracked firmware and tooling files: `app/src/main.c`, `app/prj.conf`, `app/Kconfig`, `app/CMakeLists.txt`, `app/sysbuild.conf`, `app/wifi.secrets.conf.example`, `scripts/*.sh`, `README.md`, and `.gitignore`.
- This document separates directly confirmed concerns from runtime or scaling inferences.

## Confirmed Concerns

### 1. Boot can block forever and there is no recovery loop
- In `app/src/main.c`, `wait_for_wifi_ready_state()` waits with `k_sem_take(..., K_FOREVER)`, so there is no upper bound if the Wi-Fi subsystem never becomes ready.
- In `app/src/main.c`, `main()` performs a single connect → DHCP → reachability flow, returns on first failure, and after success only sleeps in an infinite loop.
- In `app/src/main.c`, `handle_wifi_disconnect_result()` clears state and logs the event, but it does not trigger reconnect or re-run the readiness path.
- Practical impact: a transient readiness or network failure can leave the device stuck until manual reset.

### 2. App readiness depends on an external host and an unbounded socket path
- In `app/src/main.c`, `run_reachability_check()` resolves `CONFIG_APP_REACHABILITY_HOST` and opens a TCP connection to port `80` before the app declares `APP_READY`.
- The same function in `app/src/main.c` does not set an explicit DNS or socket timeout, and it does not retry with backoff.
- `app/Kconfig` and `app/wifi.secrets.conf.example` make this host configurable, but the readiness model is still coupled to an external dependency rather than only local Wi-Fi association.
- Practical impact: application startup is tied to internet path availability and stack defaults, not just Wi-Fi link health.

### 3. Wi-Fi credentials are injected as build-time config strings
- In `app/Kconfig`, `APP_WIFI_SSID`, `APP_WIFI_PSK`, and `APP_WIFI_SECURITY` are plain Kconfig strings.
- In `scripts/build.sh`, the build injects `-DEXTRA_CONF_FILE="$SECRETS_CONF"`, which points at `app/wifi.secrets.conf` by default via `scripts/common.sh`.
- In `app/src/main.c`, the firmware reads `CONFIG_APP_WIFI_SSID` and `CONFIG_APP_WIFI_PSK` directly when building the connect request.
- `.gitignore` excludes `app/wifi.secrets.conf`, which helps with Git hygiene, but the credential flow is still firmware-image and build-artifact sensitive.
- Practical impact: rotating credentials requires rebuild/reflash, and copied firmware/build outputs should be treated as sensitive.

### 4. Multi-device workflows default to the first detected device
- In `scripts/common.sh`, `detect_jlink_serial()` parses `nrfutil` output and unconditionally selects `devices[0]`.
- In `scripts/common.sh`, `detect_console_port()` also picks serial data from `devices[0]`.
- In `scripts/console.sh`, the fallback path chooses the first `/dev/cu.usbmodem*` candidate when multiple ports exist.
- In `scripts/flash.sh`, auto-detection is used whenever `JLINK_SERIAL` is unset.
- Practical impact: on a bench with multiple Nordic boards attached, flash or console operations can target the wrong device.

### 5. Bootstrap can report success after partial installation failure
- In `scripts/bootstrap-macos.sh`, several critical steps are intentionally non-fatal: `brew install --cask segger-jlink || true`, `install_local_jlink || true`, and `maybe_add_jlink_to_path || true`.
- The script still ends by printing `Bootstrap complete` even if J-Link installation or PATH wiring failed.
- Practical impact: developers can believe setup succeeded while flashing/debugging still fails later.

### 6. The development workflow is strongly macOS-specific
- `scripts/bootstrap-macos.sh` exits unless the host is `Darwin`.
- `scripts/doctor.sh` expects `brew`, `ioreg`, and `/dev/cu.usbmodem*` device names.
- `scripts/common.sh` searches hard-coded macOS application paths under `/Applications/SEGGER`.
- Practical impact: Linux/Windows development and CI runners require parallel tooling or custom replacements.

### 7. Zephyr/NCS warning signals are intentionally suppressed
- In `app/prj.conf`, `CONFIG_WARN_EXPERIMENTAL=n` and `CONFIG_WARN_DEPRECATED=n` disable warnings that normally help with SDK migration and option review.
- Practical impact: upgrade risk moves from early warning time into later build/runtime failure time.

### 8. Automated verification is missing
- The tracked repository content is limited to firmware sources, configs, and helper scripts; there is no `tests/` tree and no CI workflow/config files present alongside `app/*` and `scripts/*`.
- There is no scripted smoke target that validates `scripts/build.sh`, `scripts/flash.sh`, or `app/src/main.c` behavior in a repeatable way.
- Practical impact: regressions in bring-up flow, tooling, or config changes will be caught manually and late.

### 9. Project documentation is too thin for hardware bring-up
- `README.md` is only a short project label and device note; it does not document prerequisites, bootstrap, fetching the SDK, building, flashing, opening the console, or handling `app/wifi.secrets.conf`.
- Practical impact: successful onboarding depends on reverse-engineering `scripts/*.sh` or prior team knowledge.

### 10. Host-tool parsing logic is duplicated and brittle
- In `scripts/common.sh`, both `detect_jlink_serial()` and `detect_console_port()` contain their own inline Python JSON-stream parsers for `nrfutil --json-pretty device list` output.
- The duplicated shell-to-Python parsing path means output-shape changes or bug fixes must be handled in multiple places.
- Practical impact: maintenance cost rises when `nrfutil` output changes or when device detection needs refinement.

## Likely Concerns / Inferences

### A. Serial logs may expose deployment metadata
- Grounding: `app/src/main.c` logs the SSID, DHCP-leased IPv4 address, disconnect status, and reachability-check activity.
- Inference: on shared lab benches or captured serial sessions, this can leak useful environment details even though the PSK itself is not printed.

### B. The current single-file firmware layout will be harder to extend
- Grounding: almost all runtime behavior sits in the 402-line `app/src/main.c`.
- Inference: adding provisioning, reconnect policy, telemetry, or OTA logic will be harder without splitting Wi-Fi state management, reachability, and lifecycle code into separate modules.

### C. Network and memory tuning lack visible justification
- Grounding: `app/prj.conf` hard-codes packet counts, buffer counts, heap sizes, stack sizes, and TCP behavior.
- Inference: these values may be correct today, but there is no benchmark, memory budget, or validation note showing why they are safe across SDK updates or workload changes.

### D. Disabling TCP TIME_WAIT may hide churn-related issues
- Grounding: `app/prj.conf` sets `CONFIG_NET_TCP_TIME_WAIT_DELAY=0`.
- Inference: this may be a deliberate embedded optimization, but it can also make repeated-connect edge cases harder to observe during stress testing.

## Highest-value follow-ups
- Add a bounded ready/connect/reachability state machine in `app/src/main.c`, including reconnect/backoff behavior after disconnects.
- Make device selection explicit in `scripts/flash.sh` and `scripts/console.sh` whenever more than one board is visible.
- Treat `app/wifi.secrets.conf` plus generated build artifacts as sensitive inputs/outputs, and document that flow clearly in `README.md`.
- Add at least one repeatable verification path for the repo: shell linting for `scripts/*.sh`, plus a smoke build or config-validation step.
- Expand `README.md` so setup and recovery steps are discoverable without reading shell sources.
