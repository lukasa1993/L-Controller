# Architecture

All paths below are relative to `/Users/l/_DEV/LNH-Nordic`.

## Overall Shape

This codebase has a deliberately simple architecture: one Zephyr/Nordic firmware application in `app/`, plus a thin set of host-side automation scripts in `scripts/`.

There are no internal libraries, no multiple firmware modules, and no layered source tree under `app/src/`. Most runtime behavior lives in a single translation unit, `app/src/main.c`, while the rest of the repository exists to configure, build, flash, and observe that application.

The practical architectural split is:

1. Host automation layer in `scripts/`
2. Build and configuration layer in `app/CMakeLists.txt`, `app/Kconfig`, `app/prj.conf`, `app/sysbuild.conf`, and `app/wifi.secrets.conf`
3. Device runtime layer in `app/src/main.c`
4. External SDK/toolchain layer in `.ncs/`, `.venv/`, and `.tools/` (all local-only and ignored)

## Entry Points

### Developer and build entry points

- `scripts/bootstrap-macos.sh` installs host prerequisites, creates `.venv/`, and prepares local tooling.
- `scripts/fetch-ncs.sh` initializes or refreshes the Nordic SDK workspace in `.ncs/v3.2.1/` using `west`.
- `scripts/build.sh` is the main build entry point; it sources `scripts/common.sh`, loads toolchain state, and runs `west build`.
- `scripts/flash.sh` flashes the compiled firmware with `west flash` and optional J-Link serial detection.
- `scripts/console.sh` attaches a serial console using `python -m serial.tools.miniterm`.
- `scripts/doctor.sh` is a diagnostics entry point for host tools, SDK state, and connected hardware.

### Firmware entry points

- `app/CMakeLists.txt` defines the Zephyr application and compiles `app/src/main.c`.
- `app/sysbuild.conf` turns on system-level `SB_CONFIG_WIFI_NRF70` support.
- `app/prj.conf` enables networking, Wi-Fi, logging, sockets, and memory sizing for the application image.
- `int main(void)` in `app/src/main.c` is the only runtime entry point in the firmware itself.

## Major Layers

### 1. Host automation layer

The shell layer wraps complex Nordic/Zephyr tooling behind short commands. The scripts are intentionally thin and single-purpose.

- `scripts/common.sh` is the shared shell library. It defines repo-wide paths such as `ROOT_DIR`, `WORKSPACE_DIR`, `APP_DIR`, `BUILD_DIR`, `VENV_DIR`, and `SECRETS_CONF`.
- The other scripts are command wrappers that each own one operational concern: bootstrap, fetch SDK, build, flash, console, clean, or diagnose.

This layer has no business logic about Wi-Fi behavior. Its responsibility ends at preparing the environment and invoking external tools correctly.

### 2. Build and configuration layer

This layer bridges developer input into compile-time firmware behavior.

- `app/Kconfig` defines the app-specific configuration contract: `CONFIG_APP_WIFI_SSID`, `CONFIG_APP_WIFI_PSK`, `CONFIG_APP_WIFI_SECURITY`, `CONFIG_APP_WIFI_TIMEOUT_MS`, and `CONFIG_APP_REACHABILITY_HOST`.
- `app/wifi.secrets.conf.example` shows the expected override format for local credentials.
- `app/wifi.secrets.conf` is the local, ignored overlay passed via `-DEXTRA_CONF_FILE=...` from `scripts/build.sh`.
- `app/prj.conf` enables the Zephyr/NCS features that the code depends on, including IPv4, DHCPv4, sockets, DNS, logging, and the `nRF70` Wi-Fi stack.
- `app/CMakeLists.txt` keeps compilation simple by building only `app/src/main.c`.

The important design detail here is that runtime behavior is controlled almost entirely by compile-time Kconfig values rather than a dynamic config file read on-device.

### 3. Device runtime layer

The firmware is an event-driven Wi-Fi bring-up application implemented in `app/src/main.c`.

Its responsibilities include:

- locating the Wi-Fi network interface with `net_if_get_first_wifi()`
- registering network-management callbacks for connection and DHCP events
- translating `CONFIG_APP_WIFI_SECURITY` into Zephyr `enum wifi_security_type`
- issuing the connect request with `NET_REQUEST_WIFI_CONNECT`
- waiting for asynchronous results using semaphores
- running a post-connect TCP reachability check to `CONFIG_APP_REACHABILITY_HOST`
- logging `APP_READY` once the system is connected and reachable

There is no internal service layer beneath `main.c`; the file talks directly to Zephyr networking APIs.

### 4. External platform layer

The repository depends on generated or host-local components that are not versioned in Git:

- `.ncs/v3.2.1/` is the `west` workspace containing `zephyr`, `nrf`, and related SDK repositories.
- `.venv/` contains the local Python environment that provides `west` and serial tooling.
- `.tools/` is used as a local fallback location for SEGGER/J-Link tools.
- `build/` contains generated output artifacts, usually under `build/nrf7002dk_nrf5340_cpuapp/`.

These directories are architectural dependencies even though they are intentionally excluded from source control.

## Module Boundaries

The boundary lines in this repo are straightforward.

- `scripts/common.sh` is the only reusable shell module. Other scripts should source it rather than redefining path/tool helpers.
- `scripts/build.sh` owns compilation only; it does not flash hardware or open the console.
- `scripts/flash.sh` owns deployment only; it assumes `build/` already exists.
- `scripts/console.sh` owns log streaming only; it does not try to build or flash.
- `app/src/main.c` owns all device-side orchestration; there are currently no companion files like `wifi.c`, `net.c`, or `config.c`.
- `app/Kconfig` and `app/wifi.secrets.conf(.example)` define the app’s configuration surface; `app/src/main.c` consumes that surface.

Because `app/src/main.c` is monolithic, the most important boundary today is not between source files but between synchronous orchestration code and asynchronous callback handlers inside that file.

## Control Flow

### Host-side flow

The intended operator workflow is linear:

1. Run `scripts/bootstrap-macos.sh`
2. Run `scripts/fetch-ncs.sh`
3. Populate `app/wifi.secrets.conf` from `app/wifi.secrets.conf.example`
4. Run `scripts/build.sh`
5. Run `scripts/flash.sh`
6. Run `scripts/console.sh`

This makes the shell layer a pipeline around `brew`, `nrfutil`, `west`, `JLinkExe`, and `pyserial`.

### Firmware runtime flow

`app/src/main.c` follows a clear boot sequence:

1. `main()` gets the Wi-Fi interface.
2. `register_callbacks()` installs `wifi_event_handler()`, `ipv4_event_handler()`, and the Wi-Fi-ready callback.
3. `wait_for_wifi_ready_state()` blocks until the device signals readiness.
4. `connect_wifi_once()` validates config, fills `struct wifi_connect_req_params`, and triggers `NET_REQUEST_WIFI_CONNECT`.
5. `handle_wifi_connect_result()` records connection status and releases `connect_sem`.
6. `handle_ipv4_dhcp_bound()` records the DHCP lease and releases `ipv4_sem`.
7. `wait_for_wifi_connection_and_ipv4()` turns those asynchronous events into a linear success/failure flow with a shared timeout budget.
8. `run_reachability_check()` performs DNS resolution and a TCP connect to confirm the network path is usable.
9. The app logs `APP_READY` and idles forever.

The runtime model is therefore event-driven underneath, but presented as a mostly linear boot state machine at the top level.

## Data Flow

The key data path is small and easy to trace.

- Developer-provided settings originate in `app/wifi.secrets.conf`.
- Those settings become compile-time `CONFIG_APP_*` macros through Zephyr’s configuration system.
- `connect_wifi_once()` copies those macros into `struct wifi_connect_req_params`.
- Zephyr networking emits results back through `net_mgmt` callbacks.
- Callback handlers update global state such as `wifi_connected`, `connect_status`, `ipv4_bound`, and `leased_ipv4`.
- `run_reachability_check()` consumes `CONFIG_APP_REACHABILITY_HOST` and socket APIs to validate egress connectivity.

There is no persistent storage, database, or message bus in the repo. All state is in-memory process state inside `app/src/main.c`.

## Shared Abstractions

Several abstractions are reused across the repo even though the codebase is small.

- Shell environment abstraction: `scripts/common.sh` centralizes path calculation and external-tool discovery.
- Configuration abstraction: `CONFIG_APP_*` symbols define the app contract between config files and C code.
- Concurrency abstraction: `K_SEM_DEFINE(...)` semaphores bridge asynchronous callbacks into blocking waits.
- Event abstraction: `struct net_mgmt_event_callback` decouples Wi-Fi/DHCP event sources from top-level boot logic.
- Logging abstraction: `LOG_MODULE_REGISTER(app, CONFIG_LOG_DEFAULT_LEVEL)` gives all runtime reporting one sink.

## Noteworthy Patterns

- The repository is intentionally thin; most complexity is delegated to Zephyr/NCS rather than custom code.
- The shell scripts use a common pattern: `set -euo pipefail`, compute `SCRIPT_DIR`, then `source` `scripts/common.sh`.
- Configuration secrets are kept out of Git by design through `app/wifi.secrets.conf` and `.gitignore`.
- The build is reproducible only when the local `.ncs/` workspace and host tools are present; the repo alone is not self-contained.
- `app/boards/` exists as the future extension point for board-specific overlays or configuration, but it is currently unused.
- If the firmware grows, `app/src/main.c` is the most obvious file to split into smaller modules because it currently combines config parsing, event handling, connection orchestration, and reachability testing.

## Architectural Summary

In practice, this is a single-binary Wi-Fi bring-up app wrapped by a small developer-automation shell layer. The codebase is simple enough that the most useful mental model is “one firmware file plus one shell helper library,” with Zephyr/NCS providing the real platform stack underneath.
