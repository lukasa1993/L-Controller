#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=scripts/common.sh
source "$SCRIPT_DIR/common.sh"

activate_venv
require_workspace

[ -f "$SECRETS_CONF" ] || die "Missing $SECRETS_CONF. Copy app/wifi.secrets.conf.example first."

export ZEPHYR_BASE="$WORKSPACE_DIR/zephyr"
maybe_add_jlink_to_path || true

source_toolchain_env || die "No Zephyr/NCS toolchain detected. Run scripts/fetch-ncs.sh or install a Zephyr SDK manually."

mkdir -p "$BUILD_DIR"
cd "$WORKSPACE_DIR"

log "Building $APP_DIR for $BOARD"
west build \
	-s "$APP_DIR" \
	-b "$BOARD" \
	-d "$BUILD_DIR" \
	-p always \
	--sysbuild \
	-- \
	-DEXTRA_CONF_FILE="$SECRETS_CONF"

log "Build complete: $BUILD_DIR"

for artifact in \
	"$BUILD_DIR/merged.hex" \
	"$BUILD_DIR/app/zephyr/zephyr.signed.bin" \
	"$BUILD_DIR/app/zephyr/zephyr.signed.hex" \
	"$BUILD_DIR/app/zephyr/app_update.bin"; do
	if [ -f "$artifact" ]; then
		log "Artifact ready: $artifact"
	fi
done
