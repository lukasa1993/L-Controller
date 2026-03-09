#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
NCS_VERSION="${NCS_VERSION:-v3.2.1}"
BOARD="${BOARD:-nrf7002dk/nrf5340/cpuapp}"
WORKSPACE_DIR="${WORKSPACE_DIR:-$ROOT_DIR/.ncs/$NCS_VERSION}"
APP_DIR="${APP_DIR:-$ROOT_DIR/app}"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build/${BOARD//\//_}}"
VENV_DIR="${VENV_DIR:-$ROOT_DIR/.venv}"
SECRETS_CONF="${SECRETS_CONF:-$APP_DIR/wifi.secrets.conf}"
LOCAL_TOOLS_DIR="${LOCAL_TOOLS_DIR:-$ROOT_DIR/.tools}"

log() {
	printf '[%s] %s\n' "$(basename "$0")" "$*"
}

die() {
	printf '[%s] ERROR: %s\n' "$(basename "$0")" "$*" >&2
	exit 1
}

need_cmd() {
	command -v "$1" >/dev/null 2>&1 || die "Missing command: $1"
}

run_repo_script() {
	local script="$1"
	shift

	local script_path="$ROOT_DIR/$script"
	[ -f "$script_path" ] || die "Missing script: $script"
	[ -x "$script_path" ] || die "Script is not executable: $script"

	"$script_path" "$@"
}

print_ready_state_markers() {
	printf '%s\n' \
		'Wi-Fi connected' \
		'DHCP IPv4 address: ...' \
		'Reachability check passed' \
		'APP_READY'
}

print_phase4_persistence_markers() {
	printf '%s\n' \
		'Persistence snapshot ready (layout v..., actions=..., schedules=..., relay desired=..., relay reboot=...)' \
		'Persistence auth: loaded | empty-default (reseeded configured credentials) | corrupt-reset | incompatible-reset' \
		'Persistence actions: loaded | empty-default | corrupt-reset | incompatible-reset' \
		'Persistence relay: loaded | empty-default | corrupt-reset | incompatible-reset' \
		'Persistence schedule: loaded | empty-default | corrupt-reset | incompatible-reset'
}

print_phase4_scenario_labels() {
	printf '%s\n' \
		'clean-device defaults' \
		'reboot persistence' \
		'supported power loss durability' \
		'section corruption isolation' \
		'auth reseed' \
		'relay reboot policy'
}

print_phase4_device_checklist() {
	printf '%s\n' \
		'Re-run ./scripts/validate.sh first so the canonical automated build path is green.' \
		'Flash the latest firmware with ./scripts/flash.sh.' \
		'Open the device console with ./scripts/console.sh.' \
		'Confirm a clean device logs persistence initialization, starts with blank safe action and schedule defaults, and reseeds configured admin credentials when the auth section is absent or corrupt.' \
		'Save representative auth, relay/action, and schedule data through the implemented persistence entrypoints.' \
		'Reboot the device and confirm the saved values reload on the next boot together with Persistence snapshot ready ... and per-section persistence status logs.' \
		'Repeat the durability check with the supported power loss or power-cycle lab flow after a completed save and confirm the device does not report ambiguous half-written state.' \
		'Corrupt or remove exactly one persisted section and confirm only that section resets or reseeds while healthy sections still report loaded.' \
		'Persist a non-default relay last desired state and reboot policy, reboot again, and confirm next-boot logs or observed relay behavior match the stored policy.' \
		'Record whether the clean-device defaults, reboot persistence, supported power loss durability, section corruption isolation, auth reseed, and relay reboot policy scenarios passed or failed.'
}

maybe_add_jlink_to_path() {
	local candidate
	for candidate in \
		"$LOCAL_TOOLS_DIR/SEGGER/JLink_V*" \
		"/Applications/SEGGER/JLink" \
		"/Applications/SEGGER/JLink_V*/JLink" \
		"/Applications/SEGGER/JLink_V*"; do
		for candidate in $candidate; do
			if [ -x "$candidate/JLinkExe" ]; then
				export PATH="$candidate:$PATH"
				return 0
			fi
		done
	done

	return 1
}

nrfutil_has_command() {
	command -v nrfutil >/dev/null 2>&1 || return 1
	nrfutil list 2>/dev/null | awk '{print $1}' | grep -qx "$1"
}

detect_jlink_serial() {
	nrfutil_has_command device || return 1
	nrfutil --json-pretty device list 2>/dev/null | python3 -c '
import json, sys
payload = sys.stdin.read()
decoder = json.JSONDecoder()
index = 0
devices = []
while index < len(payload):
    while index < len(payload) and payload[index].isspace():
        index += 1
    if index >= len(payload):
        break
    obj, end = decoder.raw_decode(payload, index)
    index = end
    if obj.get("type") == "task_end":
        devices = obj.get("data", {}).get("data", {}).get("devices", [])
        break
if not devices:
    raise SystemExit(1)
serial = devices[0].get("serialNumber", "")
serial = serial.lstrip("0") or serial
print(serial)
'
}

detect_console_port() {
	nrfutil_has_command device || return 1
	nrfutil --json-pretty device list 2>/dev/null | python3 -c '
import json, sys
payload = sys.stdin.read()
decoder = json.JSONDecoder()
index = 0
devices = []
while index < len(payload):
    while index < len(payload) and payload[index].isspace():
        index += 1
    if index >= len(payload):
        break
    obj, end = decoder.raw_decode(payload, index)
    index = end
    if obj.get("type") == "task_end":
        devices = obj.get("data", {}).get("data", {}).get("devices", [])
        break
if not devices:
    raise SystemExit(1)
ports = devices[0].get("serialPorts", [])
preferred = None
for entry in ports:
    path = entry.get("path") or entry.get("comName")
    if not path:
        continue
    path = path.replace("/dev/tty.", "/dev/cu.")
    if entry.get("vcom") == 1:
        preferred = path
        break
    if preferred is None:
        preferred = path
if not preferred:
    raise SystemExit(1)
print(preferred)
'
}

source_toolchain_env() {
	local env_file

	if nrfutil_has_command toolchain-manager; then
		env_file="$(mktemp)"
		nrfutil toolchain-manager env --as-script sh --ncs-version "$NCS_VERSION" >"$env_file"
		# shellcheck disable=SC1090
		source "$env_file"
		rm -f "$env_file"
		return 0
	fi

	if [ -n "${ZEPHYR_SDK_INSTALL_DIR:-}" ] || command -v arm-zephyr-eabi-gcc >/dev/null 2>&1; then
		if [ -z "${ZEPHYR_TOOLCHAIN_VARIANT:-}" ]; then
			export ZEPHYR_TOOLCHAIN_VARIANT=zephyr
		fi

		if [ -z "${ZEPHYR_SDK_INSTALL_DIR:-}" ] && command -v arm-zephyr-eabi-gcc >/dev/null 2>&1; then
			export ZEPHYR_SDK_INSTALL_DIR="$(cd "$(dirname "$(command -v arm-zephyr-eabi-gcc)")/../.." && pwd)"
		fi

		return 0
	fi

	return 1
}

activate_venv() {
	[ -f "$VENV_DIR/bin/activate" ] || die "Missing virtualenv at $VENV_DIR. Run scripts/bootstrap-macos.sh first."
	# shellcheck disable=SC1090
	source "$VENV_DIR/bin/activate"
}

west_cmd() {
	activate_venv
	need_cmd west
	west "$@"
}

require_workspace() {
	[ -d "$WORKSPACE_DIR/.west" ] || die "Missing NCS workspace at $WORKSPACE_DIR. Run scripts/fetch-ncs.sh first."
	[ -d "$WORKSPACE_DIR/nrf" ] || die "Missing nrf repository in $WORKSPACE_DIR."
	[ -d "$WORKSPACE_DIR/zephyr" ] || die "Missing zephyr repository in $WORKSPACE_DIR."
}

serial_candidates() {
	local pattern="${1:-/dev/cu.usbmodem*}"
	shopt -s nullglob
	for device in $pattern; do
		printf '%s\n' "$device"
	done
	shopt -u nullglob
}
