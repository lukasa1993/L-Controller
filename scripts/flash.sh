#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=scripts/common.sh
source "$SCRIPT_DIR/common.sh"

activate_venv
require_workspace
maybe_add_jlink_to_path || true
need_cmd JLinkExe

[ -d "$BUILD_DIR" ] || die "Missing build directory $BUILD_DIR. Run scripts/build.sh first."

if [ -z "${JLINK_SERIAL:-}" ]; then
	JLINK_SERIAL="$(detect_jlink_serial || true)"
	if [ -n "$JLINK_SERIAL" ]; then
		log "Using detected J-Link serial $JLINK_SERIAL"
	fi
fi

cd "$WORKSPACE_DIR"
log "Flashing $BOARD from $BUILD_DIR"
flash_args=(flash --build-dir "$BUILD_DIR" --runner jlink)

if [ -n "${JLINK_SERIAL:-}" ]; then
	flash_args+=(--dev-id "$JLINK_SERIAL")
fi

west "${flash_args[@]}"
