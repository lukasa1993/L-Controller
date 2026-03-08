# Technology Stack

## Repository Snapshot
- This repository is a compact embedded firmware project: the product code lives in `app/`, while developer workflows live in `scripts/`.
- The main application entrypoint is `app/src/main.c`.
- The Zephyr project is declared in `app/CMakeLists.txt` as `project(lnh_nordic_wifi)`.
- Shared environment defaults are centralized in `scripts/common.sh`.
- The repo already contains a local Nordic SDK workspace under `.ncs/v3.2.1/` and a local SEGGER tool cache under `.tools/SEGGER/`.

## Languages
- **C** is the main product language. All firmware logic currently lives in `app/src/main.c`.
- **Bash** powers the workstation scripts in `scripts/bootstrap-macos.sh`, `scripts/fetch-ncs.sh`, `scripts/build.sh`, `scripts/flash.sh`, `scripts/console.sh`, `scripts/doctor.sh`, and `scripts/clean.sh`.
- **Kconfig** is used for app and system configuration in `app/Kconfig`, `app/prj.conf`, and `app/sysbuild.conf`.
- **CMake** is the build entrypoint in `app/CMakeLists.txt`.
- **Python** is a tooling dependency rather than an app language: it is used for the local virtualenv, `west`, `pyserial`, SDK requirement installs, and inline JSON parsing in `scripts/common.sh`.

## Runtime Targets
- The embedded runtime is **Zephyr RTOS**, loaded by `find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})` in `app/CMakeLists.txt`.
- The pinned Nordic SDK version is **nRF Connect SDK `v3.2.1`**, set in `scripts/common.sh` and confirmed by `.ncs/v3.2.1/nrf/VERSION`.
- The vendored Zephyr tree under `.ncs/v3.2.1/zephyr/` reports version `4.2.99` in `.ncs/v3.2.1/zephyr/VERSION`.
- The default board target is `nrf7002dk/nrf5340/cpuapp`, defined in `scripts/common.sh`.
- Default build output goes to `build/nrf7002dk_nrf5340_cpuapp/` via `BUILD_DIR="${ROOT_DIR}/build/${BOARD//\//_}"` in `scripts/common.sh`.

## Frameworks and Core Libraries
- **Zephyr kernel APIs** are used directly in `app/src/main.c` for semaphores, timeouts, sleep, and uptime handling.
- **Zephyr networking** is the central application subsystem: `app/src/main.c` includes `zephyr/net/net_if.h`, `zephyr/net/net_mgmt.h`, `zephyr/net/socket.h`, and `zephyr/net/wifi_mgmt.h`.
- **Nordic Wi-Fi support** is enabled in `app/prj.conf` with `CONFIG_WIFI_NRF70=y`.
- **WPA supplicant integration** is enabled with `CONFIG_WIFI_NM_WPA_SUPPLICANT=y` in `app/prj.conf`.
- **Nordic Wi-Fi ready library** is enabled with `CONFIG_WIFI_READY_LIB=y` and used through `#include <net/wifi_ready.h>` in `app/src/main.c`.
- **Zephyr logging** is enabled in `app/prj.conf` and used through `LOG_INF`, `LOG_WRN`, and `LOG_ERR` in `app/src/main.c`.
- There is no higher-level application framework on top of Zephyr: no HTTP framework, no UI layer, and no backend service code in this repo.

## Build System and Dependency Management
- `west` is the main build orchestrator. `scripts/build.sh` runs `west build` against `app/`.
- The build is always pristine because `scripts/build.sh` passes `-p always`.
- The Nordic workspace is initialized by `scripts/fetch-ncs.sh` with `west init -m https://github.com/nrfconnect/sdk-nrf --mr "$NCS_VERSION" "$WORKSPACE_DIR"`.
- Dependency fetch/update is done through `west update --fetch smart --narrow -o=--depth=1` in `scripts/fetch-ncs.sh`.
- Python build dependencies are installed from `.ncs/v3.2.1/zephyr/scripts/requirements.txt` and `.ncs/v3.2.1/nrf/scripts/requirements.txt`.
- The local Python environment lives in `.venv/` and is activated by `activate_venv()` in `scripts/common.sh`.
- `scripts/bootstrap-macos.sh` installs `west` and `pyserial` into that virtualenv.

## Host Tooling
- Core host tools checked by `scripts/doctor.sh` are `brew`, `git`, `cmake`, `python3`, `ninja`, and `nrfutil`.
- Extra Homebrew packages installed by `scripts/bootstrap-macos.sh` are `ninja`, `gperf`, `ccache`, and `dtc`.
- Flashing requires `JLinkExe`, enforced by `scripts/flash.sh`.
- Serial monitoring uses `python -m serial.tools.miniterm` in `scripts/console.sh`.
- `scripts/common.sh` can source the Nordic toolchain from `nrfutil toolchain-manager env --as-script sh --ncs-version "$NCS_VERSION"`.
- If `nrfutil` toolchain activation is unavailable, `scripts/common.sh` falls back to a manually installed Zephyr SDK via `ZEPHYR_SDK_INSTALL_DIR` or `arm-zephyr-eabi-gcc`.

## Application Configuration Surface
- App-specific Kconfig symbols are declared in `app/Kconfig`.
- The main product-facing settings are `CONFIG_APP_WIFI_SSID`, `CONFIG_APP_WIFI_PSK`, `CONFIG_APP_WIFI_SECURITY`, `CONFIG_APP_WIFI_TIMEOUT_MS`, and `CONFIG_APP_REACHABILITY_HOST`.
- Base system capabilities are enabled in `app/prj.conf`.
- Key network flags in `app/prj.conf` include `CONFIG_NETWORKING=y`, `CONFIG_NET_IPV4=y`, `CONFIG_NET_DHCPV4=y`, `CONFIG_NET_SOCKETS=y`, and `CONFIG_DNS_RESOLVER=y`.
- Key Wi-Fi flags in `app/prj.conf` include `CONFIG_WIFI=y`, `CONFIG_WIFI_NRF70=y`, `CONFIG_WIFI_NM_WPA_SUPPLICANT=y`, and `CONFIG_WIFI_READY_LIB=y`.
- Memory/stack tuning is explicit in `app/prj.conf`, for example `CONFIG_MAIN_STACK_SIZE=6144`, `CONFIG_HEAP_MEM_POOL_SIZE=43000`, and `CONFIG_NRF_WIFI_DATA_HEAP_SIZE=40000`.
- Sysbuild integration is minimal but present: `app/sysbuild.conf` sets `SB_CONFIG_WIFI_NRF70=y`.

## Secrets and Local Overrides
- Developer-local Wi-Fi configuration is expected in `app/wifi.secrets.conf`.
- The committed template is `app/wifi.secrets.conf.example`.
- `scripts/build.sh` injects that overlay with `-DEXTRA_CONF_FILE="$SECRETS_CONF"`.
- `.gitignore` excludes `app/wifi.secrets.conf`, `.venv/`, `.ncs/`, `.tools/`, and `build/`.
- This means the repo is designed to keep credentials and heavy SDK/tool binaries out of git while still allowing a fully local workstation setup.

## Workspace and Infra Layout
- `.ncs/v3.2.1/.west/config` points the west manifest to `nrf` and the Zephyr base to `zephyr`.
- `.ncs/v3.2.1/` contains the fetched SDK workspace rather than just a manifest lockfile.
- `.tools/SEGGER/` is used as a local fallback install location for SEGGER tools.
- `scripts/bootstrap-macos.sh` can populate `.tools/SEGGER/` by extracting a cached Homebrew J-Link package when a global install is missing.
- The repo currently has no container image, no devcontainer, and no CI configuration in the root.

## Practical Commands
- Bootstrap a macOS workstation: `scripts/bootstrap-macos.sh`
- Fetch or refresh the SDK workspace: `scripts/fetch-ncs.sh`
- Build the firmware: `scripts/build.sh`
- Flash the board: `scripts/flash.sh`
- Open the serial console: `scripts/console.sh`
- Validate workstation/device prerequisites: `scripts/doctor.sh`
- Remove local build artifacts: `scripts/clean.sh`

## Common Overrides
- Override the target board by exporting `BOARD` before running `scripts/build.sh` or `scripts/flash.sh`.
- Override the SDK version by exporting `NCS_VERSION`, which also changes the default `WORKSPACE_DIR` under `.ncs/`.
- Override the secrets overlay by exporting `SECRETS_CONF` instead of using `app/wifi.secrets.conf`.
- Override the flash target by exporting `JLINK_SERIAL` before `scripts/flash.sh`.
- Override the console port or baud rate with `SERIAL_PORT` and `SERIAL_BAUD` before `scripts/console.sh`.

## Notable Omissions
- There is no `package.json`, `pyproject.toml`, `Cargo.toml`, or `go.mod` in the repo root.
- There is no dedicated automated test directory.
- There is no backend/server component in this repository.
- There is no containerized local environment definition.
