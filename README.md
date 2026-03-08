# LNH Nordic

CLI-first bring-up for the Nordic `nrf7002dk/nrf5340/cpuapp` board target.

## What this repo contains

- A blank nRF Connect SDK application in `app/`
- Local-only Wi‑Fi secrets in `app/wifi.secrets.conf`
- Setup and workflow scripts in `scripts/`
- A gitignored SDK workspace at `.ncs/v3.2.1`
- A repo-local fallback location for J-Link tools at `.tools/`

## Happy-path setup

1. Bootstrap the Mac host tools:

   ```bash
   ./scripts/bootstrap-macos.sh
   ```

   This installs the repo-local Python environment, `west`, `nrfutil`, `nrfutil toolchain-manager`, `nrfutil device`, and a usable J-Link CLI path.

2. Fetch the pinned nRF Connect SDK workspace:

   ```bash
   ./scripts/fetch-ncs.sh
   ```

   This also installs the matching Nordic toolchain through `nrfutil toolchain-manager` when that command is available.

3. Create your local Wi‑Fi secrets file:

   ```bash
   cp app/wifi.secrets.conf.example app/wifi.secrets.conf
   ```

   Then edit `app/wifi.secrets.conf` with your SSID, password, security mode, timeout, and reachability host.

4. Run a host sanity check:

   ```bash
   ./scripts/doctor.sh
   ```

5. Build the firmware:

   ```bash
   ./scripts/build.sh
   ```

6. Flash the board:

   ```bash
   ./scripts/flash.sh
   ```

7. Open the serial console:

   ```bash
   ./scripts/console.sh
   ```

## Secrets file template

```conf
CONFIG_APP_WIFI_SSID="replace-me"
CONFIG_APP_WIFI_PSK="replace-me"
CONFIG_APP_WIFI_SECURITY="psk"
CONFIG_APP_WIFI_TIMEOUT_MS=90000
CONFIG_APP_REACHABILITY_HOST="example.com"
```

Supported `CONFIG_APP_WIFI_SECURITY` values are `none`, `psk`, `psk-sha256`, and `sae`.

## Expected success log flow

After boot and flash, the console should show a flow like this:

```text
<inf> app: Wi-Fi ready state changed: ready
<inf> app: Connecting to SSID '...'
<inf> app: Wi-Fi connected
<inf> app: DHCP IPv4 address: ...
<inf> app: Reachability check passed
<inf> app: APP_READY
```

## Common failures

- `Missing virtualenv`: run `./scripts/bootstrap-macos.sh`
- `Missing NCS workspace`: run `./scripts/fetch-ncs.sh`
- `Missing app/wifi.secrets.conf`: copy the example file and fill in credentials
- `No Zephyr/NCS toolchain detected`: re-run `./scripts/fetch-ncs.sh` to install the matching `nrfutil` toolchain, or provide a Zephyr SDK in your environment
- `JLinkExe missing`: `./scripts/bootstrap-macos.sh` tries Homebrew first, then extracts a local copy into `.tools/` if admin installation is unavailable
- First J-Link connection may take longer than usual while the on-board probe updates its firmware
- Multiple `/dev/cu.usbmodem*` devices: set `SERIAL_PORT=/dev/cu.usbmodem...` before running `./scripts/console.sh`

## Script interface

- `./scripts/bootstrap-macos.sh`
- `./scripts/fetch-ncs.sh`
- `./scripts/doctor.sh`
- `./scripts/build.sh`
- `./scripts/flash.sh`
- `./scripts/console.sh`
- `./scripts/clean.sh`
