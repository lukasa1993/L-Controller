#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=scripts/common.sh
source "$SCRIPT_DIR/common.sh"

[ "$(uname -s)" = "Darwin" ] || die "This bootstrap script only supports macOS."
need_cmd brew
need_cmd python3

install_local_jlink() {
	local cache_pkg
	local temp_dir
	local source_dir

	cache_pkg="$(ls -t "$HOME"/Library/Caches/Homebrew/downloads/*--JLink_MacOSX_V*_universal.pkg 2>/dev/null | head -n 1 || true)"
	[ -n "$cache_pkg" ] || return 1

	temp_dir="$(mktemp -d)"
	pkgutil --expand-full "$cache_pkg" "$temp_dir/pkg" >/dev/null
	source_dir="$(ls -d "$temp_dir"/pkg/JLink.pkg/Payload/Applications/SEGGER/JLink_V* 2>/dev/null | head -n 1 || true)"
	[ -n "$source_dir" ] || return 1

	mkdir -p "$LOCAL_TOOLS_DIR/SEGGER"
	rm -rf "$LOCAL_TOOLS_DIR/SEGGER/JLink_V"*
	cp -R "$source_dir" "$LOCAL_TOOLS_DIR/SEGGER/"
	rm -rf "$temp_dir"
	return 0
}

log "Installing Homebrew prerequisites"
brew install ninja gperf ccache dtc

if ! command -v nrfutil >/dev/null 2>&1; then
	log "Installing nrfutil"
	brew install --cask nrfutil
fi

if ! nrfutil_has_command toolchain-manager; then
	log "Installing nrfutil toolchain-manager command"
	nrfutil install toolchain-manager
fi

if ! nrfutil_has_command device; then
	log "Installing nrfutil device command"
	nrfutil install device
fi

if ! command -v JLinkExe >/dev/null 2>&1; then
	log "Installing SEGGER J-Link tools"
	brew install --cask segger-jlink || true
	if ! maybe_add_jlink_to_path; then
		log "Falling back to local J-Link extraction under $LOCAL_TOOLS_DIR"
		install_local_jlink || true
		maybe_add_jlink_to_path || true
	fi
fi

if [ ! -d "$VENV_DIR" ]; then
	log "Creating Python virtualenv at $VENV_DIR"
	python3 -m venv "$VENV_DIR"
fi

activate_venv
python -m pip install --upgrade pip setuptools wheel
python -m pip install west pyserial

maybe_add_jlink_to_path || true

log "Bootstrap complete"
printf '  venv      %s\n' "$VENV_DIR"
printf '  workspace %s\n' "$WORKSPACE_DIR"
printf '  west      %s\n' "$(command -v west)"
printf '  nrfutil   %s\n' "$(command -v nrfutil || echo missing)"
printf '  tcm       %s\n' "$(nrfutil_has_command toolchain-manager && echo installed || echo missing)"
printf '  device    %s\n' "$(nrfutil_has_command device && echo installed || echo missing)"
printf '  JLinkExe  %s\n' "$(command -v JLinkExe || echo missing)"
