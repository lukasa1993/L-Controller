#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=scripts/common.sh
source "$SCRIPT_DIR/common.sh"

activate_venv
need_cmd git
need_cmd python

if [ ! -d "$WORKSPACE_DIR/.west" ]; then
	log "Initializing nRF Connect SDK workspace at $WORKSPACE_DIR"
	mkdir -p "$(dirname "$WORKSPACE_DIR")"
	west init -m https://github.com/nrfconnect/sdk-nrf --mr "$NCS_VERSION" "$WORKSPACE_DIR"
else
	log "Reusing existing NCS workspace at $WORKSPACE_DIR"
fi

cd "$WORKSPACE_DIR"
log "Fetching NCS repositories"
west update --fetch smart --narrow -o=--depth=1
west zephyr-export

log "Installing Python build dependencies"
python -m pip install -r zephyr/scripts/requirements.txt -r nrf/scripts/requirements.txt

if nrfutil_has_command toolchain-manager; then
	log "Ensuring Nordic toolchain for $NCS_VERSION"
	nrfutil toolchain-manager install --ncs-version "$NCS_VERSION"
fi

log "NCS workspace ready"
printf '  manifest  %s\n' "$WORKSPACE_DIR/nrf"
printf '  zephyr    %s\n' "$WORKSPACE_DIR/zephyr"
