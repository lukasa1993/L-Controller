#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=scripts/common.sh
source "$SCRIPT_DIR/common.sh"

log "Removing local build artifacts from $ROOT_DIR/build"
rm -rf "$ROOT_DIR/build"

