#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=scripts/common.sh
source "$SCRIPT_DIR/common.sh"

show_usage() {
	printf '%s\n' \
		'Usage: ./scripts/validate.sh [--preflight]' \
		'' \
		'Runs the fast automated validation path for Phase 2 supervision work.' \
		'By default this delegates to ./scripts/build.sh only and leaves device-only verification manual.' \
		'' \
		'Options:' \
		'  --preflight  Run ./scripts/doctor.sh first for an explicit tool/device preflight.' \
		'  -h, --help   Show this help message.'
}

run_preflight=0

while [ "$#" -gt 0 ]; do
	case "$1" in
		--preflight)
			run_preflight=1
			;;
		-h|--help)
			show_usage
			exit 0
			;;
		*)
			die "Unknown option: $1"
			;;
	esac
	shift
done

log 'Phase 2 automated validation uses ./scripts/build.sh as the canonical build signal.'

if [ "$run_preflight" -eq 1 ]; then
	log 'Running optional preflight via ./scripts/doctor.sh'
	run_repo_script scripts/doctor.sh
fi

run_repo_script scripts/build.sh

printf '\n'
log 'Automated validation passed. Device-only Phase 2 supervision sign-off remains blocking:'
printf '  1. %s\n' './scripts/flash.sh'
printf '  2. %s\n' './scripts/console.sh'
printf '  3. Confirm these ready-state markers:\n'
while IFS= read -r marker; do
	printf '     - %s\n' "$marker"
done < <(print_ready_state_markers)
printf '  4. Confirm these supervisor-state markers during the device run:\n'
while IFS= read -r marker; do
	printf '     - %s\n' "$marker"
done < <(print_supervisor_state_markers)
