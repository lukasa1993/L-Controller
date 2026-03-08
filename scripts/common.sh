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
