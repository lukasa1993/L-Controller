# Testing Patterns

**Analysis Date:** 2026-03-08

## Current Testing Posture

- There is no first-party automated test suite checked into the repository. `git ls-files` contains only app/config files and operational scripts; it does not contain `tests/`, `*_test.c`, `*.spec.*`, or runner config files.
- The project currently relies on manual and hardware-assisted validation through the scripts in `scripts/` and runtime logs from `app/src/main.c`.
- The fetched SDK workspace does contain upstream tests under `.ncs/v3.2.1/test`, but `.gitignore` excludes `.ncs/`, so those are dependency tests rather than project-owned regression tests.

## Test Tooling

**Runner / framework**
- No repo-level test runner is configured: there is no `twister`, `ztest`, `ctest`, `pytest`, `jest`, `vitest`, or comparable test entrypoint in the tracked files.
- `app/CMakeLists.txt` only builds `app/src/main.c`; it does not define a separate test target.
- No CI workflow, coverage config, or badge source is present in the tracked repository.

**Lint and static signals**
- The shell scripts include `# shellcheck` annotations in files such as `scripts/build.sh`, `scripts/doctor.sh`, and `scripts/common.sh`, which suggests the scripts are expected to stay ShellCheck-friendly.
- That lint intent is not yet wired into an executable repo command or automation.

## Validation Commands in Practice

```bash
./scripts/bootstrap-macos.sh   # Install local prerequisites on macOS
./scripts/fetch-ncs.sh         # Fetch the NCS workspace and Python deps
./scripts/doctor.sh            # Validate host tools, workspace, toolchain, and hardware hints
./scripts/build.sh             # Build the firmware image using `app/wifi.secrets.conf`
./scripts/flash.sh             # Flash the image to the target board via west + J-Link
./scripts/console.sh           # Open the serial console and watch runtime behavior
./scripts/clean.sh             # Remove local build artifacts before a clean rebuild
```

- `scripts/doctor.sh` is the closest thing to a pre-test gate. It returns a failing exit code when required tools, workspaces, or hardware indicators are missing.
- `scripts/build.sh` and `scripts/flash.sh` provide the repeatable part of validation, but they still depend on local SDK/toolchain installation and attached hardware.

## Test Locations and Organization

- No first-party `tests/` directory exists next to `app/` or `scripts/`.
- `app/src/` contains only `main.c`, so there are no colocated unit tests beside the firmware source.
- No mocked device layer, fake network layer, or simulator harness is checked in.
- Practical validation is log-driven: the effective assertions come from observing messages emitted by `app/src/main.c` during a real device run.

## Effective Smoke-Test Flow

1. Prepare the machine with `scripts/bootstrap-macos.sh` and `scripts/fetch-ncs.sh`.
2. Run `scripts/doctor.sh` to confirm tools, workspace, toolchain, USB visibility, and serial port candidates.
3. Build and flash with `scripts/build.sh` and `scripts/flash.sh`.
4. Open `scripts/console.sh` and confirm the runtime sequence reaches the expected success markers from `app/src/main.c`.

Typical success signals are:

```text
Wi-Fi connected
DHCP IPv4 address: ...
Reachability check passed
APP_READY
```

- This is a hardware-in-the-loop smoke test. It validates the happy path for Wi-Fi association, DHCP, DNS, and TCP reachability, but it is not a deterministic automated regression suite.

## Mocks, Fixtures, and Inputs

- No mocking framework is present.
- No fixtures or factory directories are present.
- `app/wifi.secrets.conf.example` is a setup template for manual runs, not a test fixture in the conventional sense.
- Environment overrides in `scripts/common.sh` and `scripts/console.sh` (`BOARD`, `NCS_VERSION`, `WORKSPACE_DIR`, `SERIAL_PORT`, `SERIAL_BAUD`, `JLINK_SERIAL`) act as manual test knobs rather than formal parametrized test inputs.

## Coverage Signals

- No coverage percentage, threshold, or report artifact is configured anywhere in the tracked repo.
- No `gcov`, `lcov`, or Zephyr coverage instrumentation appears in `app/CMakeLists.txt`, `app/prj.conf`, or `scripts/`.
- The strongest runtime validation signal is the built-in reachability probe in `run_reachability_check()` inside `app/src/main.c`.
- `scripts/doctor.sh` improves confidence in the host environment, but it does not measure functional code coverage.

## Notable Gaps

- No unit tests cover helper logic like `app_security_from_kconfig()` or `remaining_timeout()` in `app/src/main.c`.
- No deterministic tests exercise the callback-driven state changes in `handle_wifi_connect_result()`, `handle_wifi_disconnect_result()`, or `handle_ipv4_dhcp_bound()`.
- No regression tests protect the inline JSON parsing performed by `detect_jlink_serial()` and `detect_console_port()` in `scripts/common.sh`.
- No automated linting is wired up for shell scripts even though ShellCheck markers are already present.
- No simulator-based or host-native test path exists, so every meaningful validation currently depends on local toolchains and physical hardware.

## Practical Guidance for Future Additions

- First-party tests should live outside `.ncs/` so they remain clearly owned by this repo.
- If shell coverage is added first, `shellcheck` is the obvious starting point because the scripts already contain targeted suppression/source annotations.
- If firmware tests are added, wire them through first-party build files such as `app/CMakeLists.txt` or a dedicated sibling test target instead of relying on the vendored SDK tree.
- Until automated tests exist, treat `scripts/doctor.sh` plus the build/flash/console flow as the de facto acceptance checklist.

*Testing analysis: 2026-03-08*
*This document is intentionally candid: testing is currently sparse and mostly manual*
