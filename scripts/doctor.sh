#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=scripts/common.sh
source "$SCRIPT_DIR/common.sh"

status=0

check_cmd() {
	local name="$1"
	local command_name="$2"
	if command -v "$command_name" >/dev/null 2>&1; then
		printf '[ok]   %-16s %s\n' "$name" "$(command -v "$command_name")"
	else
		printf '[miss] %-16s %s\n' "$name" "$command_name"
		status=1
	fi
}

echo "Host tools"
check_cmd brew brew
check_cmd git git
check_cmd cmake cmake
check_cmd python3 python3
check_cmd ninja ninja
check_cmd nrfutil nrfutil

if [ -f "$VENV_DIR/bin/west" ]; then
	printf '[ok]   %-16s %s\n' "west" "$VENV_DIR/bin/west"
else
	printf '[miss] %-16s %s\n' "west" "$VENV_DIR/bin/west"
	status=1
fi

if nrfutil_has_command toolchain-manager; then
	printf '[ok]   %-16s %s\n' "toolchain-mgr" "installed"
else
	printf '[miss] %-16s %s\n' "toolchain-mgr" "nrfutil install toolchain-manager"
	status=1
fi

if nrfutil_has_command device; then
	printf '[ok]   %-16s %s\n' "device-cmd" "installed"
else
	printf '[miss] %-16s %s\n' "device-cmd" "nrfutil install device"
	status=1
fi

maybe_add_jlink_to_path || true
if command -v JLinkExe >/dev/null 2>&1; then
	printf '[ok]   %-16s %s\n' "JLinkExe" "$(command -v JLinkExe)"
else
	printf '[miss] %-16s %s\n' "JLinkExe" "Install SEGGER J-Link tools"
	status=1
fi

echo
echo "SDK workspace"
if [ -d "$WORKSPACE_DIR/.west" ]; then
	printf '[ok]   %-16s %s\n' "workspace" "$WORKSPACE_DIR"
else
	printf '[miss] %-16s %s\n' "workspace" "$WORKSPACE_DIR"
	status=1
fi

for dir in nrf zephyr; do
	if [ -d "$WORKSPACE_DIR/$dir" ]; then
		printf '[ok]   %-16s %s\n' "$dir" "$WORKSPACE_DIR/$dir"
	else
		printf '[miss] %-16s %s\n' "$dir" "$WORKSPACE_DIR/$dir"
		status=1
	fi
done

echo
echo "Toolchain hints"
if nrfutil_has_command toolchain-manager; then
	versions="$(nrfutil toolchain-manager list 2>/dev/null | tr '\n' ' ' | sed 's/  */ /g')"
	if [ -n "$versions" ]; then
		printf '[ok]   %-16s %s\n' "nrfutil env" "$versions"
	else
		printf '[warn] %-16s %s\n' "nrfutil env" "No toolchain installed for $NCS_VERSION yet"
		status=1
	fi
elif command -v arm-zephyr-eabi-gcc >/dev/null 2>&1; then
	printf '[ok]   %-16s %s\n' "arm-zephyr-eabi" "$(command -v arm-zephyr-eabi-gcc)"
elif [ -n "${ZEPHYR_SDK_INSTALL_DIR:-}" ] && [ -d "$ZEPHYR_SDK_INSTALL_DIR" ]; then
	printf '[ok]   %-16s %s\n' "ZEPHYR_SDK" "$ZEPHYR_SDK_INSTALL_DIR"
else
	printf '[warn] %-16s %s\n' "toolchain" "Install a Zephyr/NCS toolchain or use nrfutil toolchain-manager"
	status=1
fi

echo
echo "USB and serial"
if ioreg -p IOUSB -l -w0 | grep -Eqi 'SEGGER|J-Link|nRF|Nordic'; then
	printf '[ok]   %-16s %s\n' "USB probe" "Detected Nordic/SEGGER USB device"
else
	printf '[warn] %-16s %s\n' "USB probe" "No Nordic/SEGGER USB device detected"
	status=1
fi

ports=()
while IFS= read -r port; do
	ports+=("$port")
done < <(serial_candidates '/dev/cu.usbmodem*')

if [ "${#ports[@]}" -gt 0 ]; then
	printf '[ok]   %-16s %s\n' "serial ports" "${#ports[@]} candidate(s)"
	printf '       %s\n' "${ports[@]}"
else
	printf '[warn] %-16s %s\n' "serial ports" "No /dev/cu.usbmodem* devices found"
	status=1
fi

exit "$status"
