# External Integrations

## Integration Overview
- This codebase integrates more with **device tooling and network infrastructure** than with SaaS backends.
- The runtime flow in `app/src/main.c` is: wait for Wi-Fi readiness, connect to an access point, wait for DHCP, resolve a hostname, then open a TCP connection to a configured host on port `80`.
- The developer flow in `scripts/` is: bootstrap host tools, fetch the Nordic SDK workspace, build with `west`, flash over J-Link, then inspect logs over USB serial.

## SDK and Upstream Source Integrations
- `scripts/fetch-ncs.sh` pulls the Nordic SDK manifest from `https://github.com/nrfconnect/sdk-nrf` using `west init`.
- The local west workspace is rooted at `.ncs/v3.2.1/`.
- `.ncs/v3.2.1/.west/config` sets the manifest path to `.ncs/v3.2.1/nrf/` and the Zephyr base to `.ncs/v3.2.1/zephyr/`.
- `west update` then populates additional upstream content such as `.ncs/v3.2.1/zephyr/` and `.ncs/v3.2.1/nrfxlib/`.
- Python-side SDK dependencies are installed from `.ncs/v3.2.1/zephyr/scripts/requirements.txt` and `.ncs/v3.2.1/nrf/scripts/requirements.txt`.
- These are setup-time integrations only; the shipped firmware does not directly call GitHub APIs.

## Host Tooling Integrations
- `scripts/bootstrap-macos.sh` integrates with **Homebrew** to install host build tools and casks.
- The same script installs **`nrfutil`** if it is missing and then installs the `toolchain-manager` and `device` commands.
- `scripts/common.sh` uses `nrfutil list` to verify command availability.
- Toolchain activation can come from `nrfutil toolchain-manager env --as-script sh --ncs-version "$NCS_VERSION"`.
- `scripts/doctor.sh` uses `nrfutil toolchain-manager list` as a readiness check for available toolchains.
- These are workstation integrations, not runtime firmware protocols.

## Flashing and Probe Integration
- Flashing is done through **SEGGER J-Link**.
- `scripts/flash.sh` hard-requires `JLinkExe` and runs `west flash --runner jlink`.
- `scripts/common.sh` looks for J-Link installs in `/Applications/SEGGER/` and in the repo-local `.tools/SEGGER/` cache.
- `scripts/bootstrap-macos.sh` can extract a Homebrew-cached J-Link package into `.tools/SEGGER/` if a normal install path is unavailable.
- `scripts/common.sh` also uses `nrfutil --json-pretty device list` plus inline Python to auto-detect a J-Link serial number.
- A practical override path exists through the `JLINK_SERIAL` environment variable.

## Serial Console Integration
- `scripts/console.sh` opens the board console with `python -m serial.tools.miniterm`.
- The default serial transport is any `/dev/cu.usbmodem*` device, resolved either from `nrfutil device list` or a filesystem glob.
- `scripts/doctor.sh` checks for USB-attached Nordic/SEGGER hardware with `ioreg -p IOUSB -l -w0 | grep -Eqi 'SEGGER|J-Link|nRF|Nordic'`.
- This means the repo assumes a local USB-connected development board rather than a remote flashing service.

## Wi-Fi Network Integration
- The runtime firmware is explicitly a Wi-Fi bring-up app, implemented in `app/src/main.c`.
- Credentials are supplied from `app/wifi.secrets.conf` or the template `app/wifi.secrets.conf.example`.
- Supported Wi-Fi security modes come from `CONFIG_APP_WIFI_SECURITY` in `app/Kconfig`: `none`, `psk`, `psk-sha256`, and `sae`.
- `app/src/main.c` converts those string values into Zephyr `enum wifi_security_type` values before calling `NET_REQUEST_WIFI_CONNECT`.
- Wi-Fi subsystem support is enabled in `app/prj.conf` with `CONFIG_WIFI=y`, `CONFIG_WIFI_NRF70=y`, `CONFIG_WIFI_NM_WPA_SUPPLICANT=y`, and `CONFIG_WIFI_READY_LIB=y`.
- The app registers for `NET_EVENT_WIFI_CONNECT_RESULT` and `NET_EVENT_WIFI_DISCONNECT_RESULT` to track link status.

## IP, DHCP, DNS, and Socket-Level Reachability
- IPv4 is enabled in `app/prj.conf` with `CONFIG_NET_IPV4=y`.
- DHCPv4 is enabled in `app/prj.conf` with `CONFIG_NET_DHCPV4=y`.
- DNS resolution is enabled in `app/prj.conf` with `CONFIG_DNS_RESOLVER=y`.
- `app/src/main.c` waits for `NET_EVENT_IPV4_DHCP_BOUND` before proceeding.
- After the interface gets an address, `app/src/main.c` calls `getaddrinfo(CONFIG_APP_REACHABILITY_HOST, "80", ...)`.
- It then attempts a raw TCP `connect()` to the resolved endpoint on port `80`.
- The committed example host is `example.com`, configured in `app/wifi.secrets.conf.example`.
- This is a connectivity probe only: there is no HTTP request, no response parsing, and no TLS handshake in the current code.

## Secrets and Credentials Handling
- Secrets are supplied by a local overlay file at `app/wifi.secrets.conf`.
- The secrets file is ignored by `.gitignore`.
- `scripts/build.sh` passes that file into the build with `-DEXTRA_CONF_FILE="$SECRETS_CONF"`.
- The current local `app/wifi.secrets.conf` contains Wi-Fi values plus extra admin-style credential keys.
- Those extra admin keys are **not** declared in `app/Kconfig` and are **not** referenced in `app/src/main.c`, so they appear unused by the present firmware.
- There is no external secret manager, vault, or cloud parameter store integration.

## Auth and Identity
- Present: Wi-Fi-level authentication to the local wireless network using open, PSK, PSK-SHA256, or SAE modes.
- Absent: application login, user database, OAuth, OpenID Connect, JWT handling, API keys, or mutual-TLS device identity.
- Absent: provisioning/commissioning service outside of the normal Wi-Fi credentials loaded from `app/wifi.secrets.conf`.
- The repo currently has no implemented app-layer use for the extra admin credential fields in `app/wifi.secrets.conf`.

## Databases, Storage, and State
- No external database integration is present.
- No local embedded database is present.
- No object storage or file-storage service integration is present.
- No Zephyr settings/NVS/filesystem layer appears to be configured in `app/prj.conf` or used from `app/src/main.c`.
- Runtime state is held in memory only through variables like `wifi_connected`, `ipv4_bound`, and `leased_ipv4` in `app/src/main.c`.

## Webhooks, Analytics, Messaging, and Payments
- Webhooks: none found.
- Analytics/telemetry backend: none found.
- Messaging/pub-sub (MQTT, AMQP, Kafka, Redis streams, etc.): none found.
- Email/SMS/push providers: none found.
- Payments/billing providers: none found.

## Other Integration Categories
- Cloud IoT platforms such as AWS IoT, Azure IoT, or GCP IoT are not present.
- BLE, Matter, Thread, and cellular integrations are not present in `app/`.
- REST or GraphQL API clients are not present; the network interaction stops at DNS resolution and a TCP connect.
- Web servers, webhook consumers, and backend RPC clients are not present in this repository.

## Practical Examples
- Fetch the SDK workspace from upstream: `scripts/fetch-ncs.sh`
- Build with a local Wi-Fi overlay file: `scripts/build.sh`
- Flash a specific probe: `JLINK_SERIAL=123456789 scripts/flash.sh`
- Open a specific console port: `SERIAL_PORT=/dev/cu.usbmodem12345 scripts/console.sh`
- Change the connectivity target by editing `CONFIG_APP_REACHABILITY_HOST` in `app/wifi.secrets.conf` or `app/wifi.secrets.conf.example`
