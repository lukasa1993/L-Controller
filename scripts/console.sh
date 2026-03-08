#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=scripts/common.sh
source "$SCRIPT_DIR/common.sh"

activate_venv
need_cmd python

port="${SERIAL_PORT:-${1:-}}"

if [ -z "$port" ]; then
	port="$(detect_console_port || true)"
	if [ -n "$port" ]; then
		log "Using detected console port $port"
	fi
fi

if [ -z "$port" ]; then
	pattern='/dev/cu.usbmodem*'
	if [ -n "${JLINK_SERIAL:-}" ]; then
		pattern="/dev/cu.usbmodem${JLINK_SERIAL}*"
	fi

	candidates=()
	while IFS= read -r candidate; do
		candidates+=("$candidate")
	done < <(serial_candidates "$pattern")

	[ "${#candidates[@]}" -gt 0 ] || die "No serial ports matched $pattern"
	port="${candidates[0]}"

	if [ "${#candidates[@]}" -gt 1 ]; then
		log "Multiple serial ports found; defaulting to $port"
		printf '  %s\n' "${candidates[@]}"
		log "Set SERIAL_PORT to override the default port"
	fi
fi

[ -e "$port" ] || die "Serial port not found: $port"

log "Opening $port at ${SERIAL_BAUD:-115200} baud"
python -m serial.tools.miniterm "$port" "${SERIAL_BAUD:-115200}" --eol LF
