#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=scripts/common.sh
source "$SCRIPT_DIR/common.sh"

show_usage() {
	printf '%s\n' \
		'Usage: ./scripts/validate.sh [--preflight]' \
		'' \
		'Runs the fast automated validation path for Phase 8 OTA lifecycle work.' \
		'By default this delegates to ./scripts/build.sh only and leaves browser/curl/device OTA sign-off manual.' \
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

log 'Phase 8 automated validation uses ./scripts/build.sh as the canonical sysbuild signal.'

if [ "$run_preflight" -eq 1 ]; then
	log 'Running optional preflight via ./scripts/doctor.sh'
	run_repo_script scripts/doctor.sh
fi

run_repo_script scripts/build.sh

printf '\n'
log 'Automated validation passed. Browser/curl/device Phase 8 sign-off remains blocking:'
printf '  1. %s\n' './scripts/flash.sh'
printf '  2. %s\n' './scripts/console.sh'
printf '  3. Confirm these ready-state, relay, scheduler, and OTA markers on the healthy path:\n'
{
	while IFS= read -r marker; do
		printf '     - %s\n' "$marker"
	done < <(print_phase8_ready_state_markers)
} || true
printf '  4. Run these authenticated OTA curl checks against the device IP:\n'
{
	while IFS= read -r command; do
		printf '     - %s\n' "$command"
	done < <(print_phase8_curl_commands)
} || true
printf '  5. Complete this blocking Phase 8 browser/curl/device checklist before approval:\n'
checklist_step=1
{
	while IFS= read -r item; do
		printf '     %d. %s\n' "$checklist_step" "$item"
		checklist_step=$((checklist_step + 1))
	done < <(print_phase8_device_checklist)
} || true
printf '  6. Record outcomes for these blocking Phase 8 scenarios:\n'
{
	while IFS= read -r scenario; do
		printf '     - %s\n' "$scenario"
	done < <(print_phase8_scenario_labels)
} || true
log './scripts/validate.sh remains the canonical automated validation command; Phase 8 approval stays blocked until the browser/curl/device checklist and scenario results are recorded.'
