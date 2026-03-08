# Structure

All paths below are relative to `/Users/l/_DEV/LNH-Nordic`.

## Top-Level Layout

The repository is compact. Most responsibilities are concentrated in a few top-level directories:

```text
.
├── README.md
├── app/
├── scripts/
├── .planning/
├── .ncs/          # generated SDK workspace, ignored
├── .venv/         # local Python environment, ignored
├── .tools/        # local tool cache/install area, ignored
└── build/         # generated build output, ignored
```

Only `README.md`, `app/`, and `scripts/` are core product source areas. The other directories are support or generated state.

## Responsibility Map by Directory

### `app/`

`app/` contains the Zephyr application image and all tracked firmware-side source/configuration.

- `app/CMakeLists.txt` registers the application with Zephyr and compiles `app/src/main.c`.
- `app/Kconfig` defines the app-specific configuration knobs exposed to the build.
- `app/prj.conf` enables the required Zephyr/NCS subsystems.
- `app/sysbuild.conf` enables board/system-level Wi-Fi support.
- `app/src/main.c` contains the entire device-side runtime implementation.
- `app/wifi.secrets.conf.example` is the committed template for local credentials and runtime settings.
- `app/wifi.secrets.conf` is the ignored local overlay used during builds.
- `app/boards/` exists for board-specific overlays or config fragments, but is currently empty.

This directory follows standard Zephyr application conventions rather than a custom project layout.

### `scripts/`

`scripts/` contains the developer workflow and local automation commands.

- `scripts/common.sh` is the shared shell library and the most important structural file in this directory.
- `scripts/bootstrap-macos.sh` sets up a macOS host.
- `scripts/fetch-ncs.sh` creates or updates the SDK workspace.
- `scripts/doctor.sh` validates tools, SDK presence, and device visibility.
- `scripts/build.sh` compiles the app.
- `scripts/flash.sh` flashes the board.
- `scripts/console.sh` opens the serial console.
- `scripts/clean.sh` removes local build output.

The naming pattern is consistent: imperative verb-based script names that correspond directly to a developer action.

### `.planning/`

`.planning/` holds generated reference material for planning and onboarding.

- `.planning/codebase/ARCHITECTURE.md` documents architecture.
- `.planning/codebase/STRUCTURE.md` documents layout and placement.

This area is documentation/supporting context, not runtime code.

### Local-only directories

Several directories exist to support local development but are intentionally excluded from version control.

- `.ncs/` contains the `west` workspace, including `zephyr/`, `nrf/`, `modules/`, and related SDK content.
- `.venv/` contains the Python environment used by `west` and serial tooling.
- `.tools/` stores local SEGGER/J-Link tools when they are not installed globally.
- `build/` stores generated build output, including board-specific build directories.

These directories are important operationally, but they are not part of the tracked source layout.

## File Placement Patterns

### Zephyr-standard file names stay in `app/`

The application root uses conventional Zephyr names instead of custom wrappers.

- `app/CMakeLists.txt` for build registration
- `app/Kconfig` for app-specific config symbols
- `app/prj.conf` for image configuration
- `app/sysbuild.conf` for system-build config

That makes the project easier to read for anyone familiar with Zephyr/NCS.

### Source code stays under `app/src/`

There is currently only one C source file, `app/src/main.c`. That is a strong signal that the firmware is still a small, single-purpose application.

If the repo grows, likely next placements would be files such as `app/src/wifi.c`, `app/src/reachability.c`, or headers under `app/src/` or a future `app/include/`, but those patterns do not exist yet.

### Shared shell helpers stay in `scripts/common.sh`

All shell entry points source `scripts/common.sh` for shared settings such as:

- `ROOT_DIR`
- `WORKSPACE_DIR`
- `BUILD_DIR`
- `VENV_DIR`
- `SECRETS_CONF`

That keeps path and toolchain logic out of the action-specific scripts.

### Sensitive or machine-specific config is separated

The repo uses a clear split between tracked defaults and ignored local state.

- `app/wifi.secrets.conf.example` is safe to commit and review.
- `app/wifi.secrets.conf` is ignored and should remain machine- or environment-specific.
- `.gitignore` also excludes `.ncs/`, `.venv/`, `.tools/`, and `build/`.

This placement pattern makes it clear which files are intended to travel with the repository and which are local setup artifacts.

## Where Key Responsibilities Live

- Firmware startup and Wi-Fi bring-up live in `app/src/main.c`.
- Compile-time feature enablement lives in `app/prj.conf`.
- App-specific runtime inputs live in `app/Kconfig` and `app/wifi.secrets.conf`.
- App build registration lives in `app/CMakeLists.txt`.
- SDK fetch/update logic lives in `scripts/fetch-ncs.sh`.
- Tool/bootstrap logic lives in `scripts/bootstrap-macos.sh`.
- Tool and device diagnostics live in `scripts/doctor.sh`.
- Flashing logic lives in `scripts/flash.sh`.
- Serial-console detection and attach logic live in `scripts/console.sh`.
- Shared shell path/tool helpers live in `scripts/common.sh`.

## Naming Conventions and Organization Signals

- Shell scripts use lowercase hyphenated names such as `fetch-ncs.sh` and `bootstrap-macos.sh`.
- The names are action-first, which makes the workflow discoverable from the directory listing alone.
- The firmware side uses framework-conventional filenames rather than app-branded names.
- Board-specific customization is expected to live under `app/boards/`, which is already present as a placeholder.
- Build output uses the board name encoded into the directory, for example `build/nrf7002dk_nrf5340_cpuapp/`.

## Practical Reading Order

For a new contributor, the fastest way to understand the repo structure is:

1. Read `README.md` for the one-line project purpose.
2. Read `scripts/common.sh` to understand path conventions and local workspace layout.
3. Read `scripts/build.sh`, `scripts/flash.sh`, and `scripts/console.sh` to understand the expected operator workflow.
4. Read `app/Kconfig`, `app/prj.conf`, and `app/wifi.secrets.conf.example` to understand the config surface.
5. Read `app/src/main.c` to understand the actual runtime behavior.

## Structure Summary

This repository is small enough that structure is easy to reason about: `app/` holds the product, `scripts/` holds the workflow, and hidden root directories hold generated local state. The layout is more “thin wrapper around Zephyr/NCS” than “large multi-package application,” and the directory tree reflects that clearly.
