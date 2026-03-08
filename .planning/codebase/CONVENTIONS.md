# Coding Conventions

**Analysis Date:** 2026-03-08

## Scope

- These conventions come from the tracked first-party files in `app/` and `scripts/`, especially `app/src/main.c`, `app/CMakeLists.txt`, `app/Kconfig`, `app/prj.conf`, `app/sysbuild.conf`, and `scripts/common.sh`.
- The fetched SDK workspace under `.ncs/` is intentionally out of scope for style guidance. It is ignored by `.gitignore` and should be treated as vendor code, not the local house style.

## Naming Patterns

**Files**
- Source and script files use lowercase names with simple, role-based wording: `app/src/main.c`, `scripts/build.sh`, `scripts/doctor.sh`, `scripts/fetch-ncs.sh`.
- Shell scripts are command-oriented and usually verb-first: `build.sh`, `flash.sh`, `clean.sh`, `bootstrap-macos.sh`.
- Build-time configuration follows Zephyr/NCS naming: `app/Kconfig`, `app/prj.conf`, `app/sysbuild.conf`, and `app/wifi.secrets.conf.example`.

**Functions**
- C functions use `lower_snake_case` and descriptive verb phrases: `register_callbacks`, `wait_for_wifi_ready_state`, `run_reachability_check`.
- Event-related helpers use `handle_*` or `*_handler` names: `handle_wifi_connect_result`, `wifi_event_handler`, `ipv4_event_handler`.
- Shared shell helpers in `scripts/common.sh` also use `lower_snake_case`: `need_cmd`, `source_toolchain_env`, `detect_console_port`.

**Variables and constants**
- File-scope firmware state uses `static` lower_snake_case names such as `wifi_ready`, `wifi_connected`, `connect_status`, and `leased_ipv4` in `app/src/main.c`.
- Macros and environment defaults use `UPPER_SNAKE_CASE`: `WIFI_EVENTS`, `IPV4_EVENTS`, `ROOT_DIR`, `WORKSPACE_DIR`, `SECRETS_CONF`.
- Boolean names are explicit state flags rather than abbreviations: `ipv4_bound`, `wifi_ready`.

## Code Style

**C / Zephyr style**
- Functions and control blocks place opening braces on the next line, matching the existing layout in `app/src/main.c`.
- Indentation follows Zephyr-style tabs, and wrapped arguments align under the first continued argument rather than using hanging punctuation-only lines.
- Pointers bind to the variable name (`const char *security_text`), locals are declared near the top of a function, and structs are commonly zero-initialized with `{0}`.
- Guard clauses are preferred over deep nesting; the file repeatedly exits early on invalid preconditions or failed SDK calls.

**Shell style**
- Every script starts with `#!/usr/bin/env bash` and `set -euo pipefail`.
- Scripts compute `SCRIPT_DIR` and source `scripts/common.sh` instead of duplicating environment/bootstrap logic.
- Expansions are quoted aggressively, arrays are used when argument order matters, and helper functions declare locals with `local`.
- Failure handling is explicit: prerequisites are checked early and fatal paths call `die` from `scripts/common.sh`.

**Comments**
- Comments are intentionally sparse. Most existing comments are practical linter hints such as `# shellcheck source=scripts/common.sh` and `# shellcheck disable=SC1090`.
- The repo favors descriptive names over explanatory comments. Add comments for constraints, tooling quirks, or platform-specific behavior, not for obvious control flow.

## Module and Script Patterns

- The firmware application is currently a single translation unit. `app/CMakeLists.txt` compiles only `app/src/main.c`.
- Reusable shell behavior is centralized in `scripts/common.sh`; operational entrypoints such as `scripts/build.sh`, `scripts/flash.sh`, and `scripts/console.sh` compose those helpers.
- Configuration is deliberately split by concern:
  - `app/Kconfig` defines app-level knobs exposed as `CONFIG_APP_*` symbols.
  - `app/prj.conf` enables Zephyr/Nordic features and memory/network defaults.
  - `app/sysbuild.conf` carries sysbuild-level toggles.
  - `app/wifi.secrets.conf.example` documents local secrets that should become an ignored `app/wifi.secrets.conf`.
- There are no internal utility libraries, services, or abstraction layers yet; first-party code talks directly to Zephyr/NCS APIs.

## State and Data Handling

- Compile-time configuration is the main data injection mechanism. The firmware reads `CONFIG_APP_WIFI_SSID`, `CONFIG_APP_WIFI_PSK`, `CONFIG_APP_WIFI_SECURITY`, `CONFIG_APP_WIFI_TIMEOUT_MS`, and `CONFIG_APP_REACHABILITY_HOST` directly in `app/src/main.c`.
- Asynchronous state is stored in file-scope statics and synchronized with `K_SEM_DEFINE` semaphores. Callback handlers update shared flags, and the main flow blocks on `k_sem_take(...)`.
- Timeouts are computed once and reused through the helper `remaining_timeout`, which keeps deadline math consistent during connect and DHCP waits.
- Temporary data stays stack-local (`struct wifi_connect_req_params params = {0};`, `char ip_buf[NET_IPV4_ADDR_LEN];`) instead of introducing dynamic app-level allocations.
- In shell code, environment variables are the main configuration surface. `scripts/common.sh` establishes defaults, while individual scripts honor overrides like `BOARD`, `NCS_VERSION`, `WORKSPACE_DIR`, `SERIAL_PORT`, `SERIAL_BAUD`, and `JLINK_SERIAL`.

## Error Handling and Logging

- C code follows negative-errno returns for expected failures: `-EINVAL`, `-ENODEV`, `-ETIMEDOUT`, `-EHOSTUNREACH`, and `-EIO` all appear in `app/src/main.c`.
- External calls are checked immediately with the recurring `if (ret != 0)` pattern and usually log before returning.
- Logging uses Zephyr's module logger via `LOG_MODULE_REGISTER(app, CONFIG_LOG_DEFAULT_LEVEL);` in `app/src/main.c`.
- Severity is meaningful: `LOG_INF` for milestones and state changes, `LOG_WRN` for recoverable or expected disruptions, `LOG_ERR` for failures, and `LOG_DBG` for noisy non-fatal setup details.
- Shell scripts fail fast through `die`, but use `|| true` deliberately around optional best-effort setup such as `maybe_add_jlink_to_path`.

## Representative Idioms

**Guard-clause error propagation in `app/src/main.c`**

```c
ret = register_callbacks();
if (ret != 0) {
	LOG_ERR("Failed to register callbacks: %d", ret);
	return ret;
}
```

**Shared helper + strict mode pattern in `scripts/*.sh`**

```bash
#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/common.sh"
```

## Notable Recurring Practices

- State-transition helpers are deliberately small and domain-specific: `handle_wifi_connect_result`, `handle_wifi_disconnect_result`, `handle_ipv4_dhcp_bound`.
- Preflight checks happen before side effects. Examples include validating `CONFIG_APP_WIFI_SSID` / `CONFIG_APP_WIFI_PSK` in `app/src/main.c` and verifying workspace/tool availability in `scripts/build.sh` and `scripts/doctor.sh`.
- Secrets are handled through an example-plus-ignore pattern: `app/wifi.secrets.conf.example` is committed, while `.gitignore` excludes the real `app/wifi.secrets.conf`.
- Script output is normalized through `log` and `die`, which prefix messages with the script name for easier terminal diagnosis.
- JSON parsing from `nrfutil` is delegated to short inline Python snippets in `scripts/common.sh` rather than brittle shell-only parsing.

*Convention analysis: 2026-03-08*
*Update this file when module boundaries or testable seams change*
