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

print_phase4_persistence_markers() {
	printf '%s\n' \
		'Persistence snapshot ready (layout v..., actions=..., schedules=..., relay desired=..., relay reboot=...)' \
		'Persistence auth: loaded | empty-default (reseeded configured credentials) | corrupt-reset | incompatible-reset' \
		'Persistence actions: loaded | empty-default | corrupt-reset | incompatible-reset' \
		'Persistence relay: loaded | empty-default | corrupt-reset | incompatible-reset' \
		'Persistence schedule: loaded | empty-default | corrupt-reset | incompatible-reset'
}

print_phase4_scenario_labels() {
	printf '%s\n' \
		'clean-device defaults' \
		'reboot persistence' \
		'supported power loss durability' \
		'section corruption isolation' \
		'auth reseed' \
		'relay reboot policy'
}

print_phase4_device_checklist() {
	printf '%s\n' \
		'Re-run ./scripts/validate.sh first so the canonical automated build path is green.' \
		'Flash the latest firmware with ./scripts/flash.sh.' \
		'Open the device console with ./scripts/console.sh.' \
		'Confirm a clean device logs persistence initialization, starts with blank safe action and schedule defaults, and reseeds configured admin credentials when the auth section is absent or corrupt.' \
		'Save representative auth, relay/action, and schedule data through the implemented persistence entrypoints.' \
		'Reboot the device and confirm the saved values reload on the next boot together with Persistence snapshot ready ... and per-section persistence status logs.' \
		'Repeat the durability check with the supported power loss or power-cycle lab flow after a completed save and confirm the device does not report ambiguous half-written state.' \
		'Corrupt or remove exactly one persisted section and confirm only that section resets or reseeds while healthy sections still report loaded.' \
		'Persist a non-default relay last desired state and reboot policy, reboot again, and confirm next-boot logs or observed relay behavior match the stored policy.' \
		'Record whether the clean-device defaults, reboot persistence, supported power loss durability, section corruption isolation, auth reseed, and relay reboot policy scenarios passed or failed.'
}

print_phase5_curl_commands() {
	printf '%s\n' \
		"curl -i http://<device-ip>/api/status" \
		"curl -i -c cookie.txt -H 'Content-Type: application/json' -d '{\"username\":\"...\",\"password\":\"...\"}' http://<device-ip>/api/auth/login" \
		"curl -i -b cookie.txt http://<device-ip>/api/status"
}

print_phase5_scenario_labels() {
	printf '%s\n' \
		'auth denial' \
		'login success' \
		'refresh and navigation persistence' \
		'logout revocation' \
		'cooldown recovery' \
		'concurrent sessions' \
		'reboot invalidates session' \
		'degraded styling resilience' \
		'read-only relay, scheduler, and update surfaces'
}

print_phase5_device_checklist() {
	printf '%s\n' \
		'Re-run ./scripts/validate.sh first so the canonical automated build path is green.' \
		'Flash the latest firmware with ./scripts/flash.sh.' \
		'Open the device console with ./scripts/console.sh and determine the device IP from the boot log.' \
		'Verify unauthenticated access is denied with curl -i http://<device-ip>/api/status.' \
		'Log in with curl using the configured CONFIG_APP_ADMIN_USERNAME and CONFIG_APP_ADMIN_PASSWORD values and confirm the device returns a sid cookie.' \
		'Verify curl -i -b cookie.txt http://<device-ip>/api/status succeeds after login.' \
		'Open the panel in a browser, log in, refresh, and navigate within the Phase 5 dashboard to confirm the session remains active until logout.' \
		'Trigger repeated wrong-password attempts until the cooldown appears, wait for recovery, and confirm a valid login succeeds again.' \
		'Log in from a second browser or private window and confirm both sessions remain usable independently.' \
		'Reboot the device and confirm the old browser session no longer reaches protected routes until a fresh login occurs.' \
		'Inspect the dashboard during normal and degraded-local conditions and confirm local status still renders even if Tailwind CDN styling is reduced.' \
		'Confirm relay, scheduler, and update cards remain read-only or placeholder-only in Phase 5.' \
		'Record which auth, session, cooldown, concurrent-session, reboot, degraded-styling, and read-only-surface scenarios passed or failed.'
}

print_phase6_ready_state_markers() {
	print_ready_state_markers
	printf '%s\n' \
		'Relay startup actual=... desired=... source=... reboot=... note=...'
}

print_phase6_curl_commands() {
	printf '%s\n' \
		"curl -i http://<device-ip>/api/status" \
		"curl -i -c cookie.txt -H 'Content-Type: application/json' -d '{\"username\":\"...\",\"password\":\"...\"}' http://<device-ip>/api/auth/login" \
		"curl -i -b cookie.txt http://<device-ip>/api/status" \
		"curl -i -b cookie.txt -H 'Content-Type: application/json' -d '{\"desiredState\":true}' http://<device-ip>/api/relay/desired-state" \
		"curl -i -b cookie.txt http://<device-ip>/api/status" \
		"curl -i -b cookie.txt -H 'Content-Type: application/json' -d '{\"desiredState\":false}' http://<device-ip>/api/relay/desired-state" \
		"curl -i -b cookie.txt http://<device-ip>/api/status"
}

print_phase6_scenario_labels() {
	printf '%s\n' \
		'relay on' \
		'relay off' \
		'curl parity' \
		'degraded local control' \
		'normal reboot policy' \
		'recovery-forced relay off' \
		'actual-versus-desired mismatch visibility' \
		'pending or blocked feedback'
}

print_phase6_device_checklist() {
	printf '%s\n' \
		'Re-run ./scripts/validate.sh first so the canonical automated build path is green.' \
		'Flash the latest firmware with ./scripts/flash.sh.' \
		'Open the device console with ./scripts/console.sh, determine the device IP from the boot log, and confirm the ready-state markers plus the relay startup log appear.' \
		'Log in through the existing local auth flow and confirm protected access still works before testing relay control.' \
		'Use the Phase 6 relay curl route to drive the relay on and confirm the physical relay, GET /api/status, and the live panel state all agree on actual on plus desired on.' \
		'Use the same relay curl route to drive the relay off and confirm the physical relay, GET /api/status, and the live panel state all agree on actual off plus desired off.' \
		'Open the panel in a browser, toggle the relay on and off, and confirm the control is one tap, uses no routine confirmation dialog, and shows inline pending or failure feedback while locked.' \
		'Exercise the LAN_UP_UPSTREAM_DEGRADED path and confirm authenticated relay control still works while the device remains locally reachable.' \
		'Reboot the device normally and confirm the relay plus status payload follow the configured normal-boot policy.' \
		'Trigger the recovery-reset or equivalent confirmed-fault path and confirm the next boot forces the relay off until a fresh command is issued.' \
		'Confirm the panel makes actual versus remembered desired state visible when safety policy keeps the applied relay off and shows the relevant safety note.' \
		'Record which relay on, relay off, curl parity, degraded-local control, normal reboot policy, recovery-forced off, mismatch visibility, and pending or blocked feedback scenarios passed or failed.'
}

print_phase7_ready_state_markers() {
	print_phase6_ready_state_markers
	printf '%s\n' \
		'Scheduler runtime ready schedules=... enabled=... automation=... clock=... degraded=... UTC cadence=... timeout=...ms' \
		'Scheduler recent problems=... clock=... degraded=...'
}

print_phase7_curl_commands() {
	printf '%s\n' \
		"curl -i http://<device-ip>/api/status" \
		"curl -i -c cookie.txt -H 'Content-Type: application/json' -d '{\"username\":\"...\",\"password\":\"...\"}' http://<device-ip>/api/auth/login" \
		"curl -i -b cookie.txt http://<device-ip>/api/schedules" \
		"curl -i -b cookie.txt -H 'Content-Type: application/json' -d '{\"scheduleId\":\"weekday-open\",\"cronExpression\":\"*/15 6-18 * * 1-5\",\"actionKey\":\"relay-on\",\"enabled\":true}' http://<device-ip>/api/schedules/create" \
		"curl -i -b cookie.txt http://<device-ip>/api/schedules" \
		"curl -i -b cookie.txt -H 'Content-Type: application/json' -d '{\"scheduleId\":\"weekday-open\",\"cronExpression\":\"0 7 * * 1-5\",\"actionKey\":\"relay-on\",\"enabled\":true}' http://<device-ip>/api/schedules/update" \
		"curl -i -b cookie.txt -H 'Content-Type: application/json' -d '{\"scheduleId\":\"weekday-open\",\"enabled\":false}' http://<device-ip>/api/schedules/set-enabled" \
		"curl -i -b cookie.txt -H 'Content-Type: application/json' -d '{\"scheduleId\":\"weekday-open\",\"enabled\":true}' http://<device-ip>/api/schedules/set-enabled" \
		"curl -i -b cookie.txt -H 'Content-Type: application/json' -d '{\"scheduleId\":\"weekday-open\"}' http://<device-ip>/api/schedules/delete"
}

print_phase7_scenario_labels() {
	printf '%s\n' \
		'untrusted scheduler gating' \
		'initial sync reset then degrade' \
		'schedule create' \
		'schedule edit' \
		'schedule enable' \
		'schedule disable' \
		'schedule delete' \
		'manual versus scheduled interaction' \
		'scheduled execution parity' \
		'reboot persistence with list re-check' \
		'missed job skip after trust restore' \
		'time correction future-only recompute' \
		'conflict rejection' \
		'recent problem visibility'
}

print_phase7_device_checklist() {
	printf '%s\n' \
		'Re-run ./scripts/validate.sh first so the canonical automated build path is green.' \
		'Flash the latest firmware with ./scripts/flash.sh.' \
		'Open the device console with ./scripts/console.sh, determine the device IP from the boot log, and confirm the ready-state, relay, and scheduler markers appear.' \
		'Before trusted time is available, confirm the scheduler reports untrusted or syncing state while manual relay control still remains available.' \
		'Deliberately exercise one failed initial trusted-time acquisition path and confirm the device performs one recovery reset, then comes back with scheduling inactive and visibly degraded instead of reboot-looping.' \
		'Log in through the panel or curl and confirm GET /api/schedules returns action choices, runtime summary, recent problems, and the current schedule list without exposing raw internal action IDs.' \
		'Create a near-future schedule and confirm the next run appears immediately in the panel or GET /api/schedules output.' \
		'Edit that schedule, then disable and re-enable it, and confirm each mutation is reflected in the list and next-run display.' \
		'Delete one schedule and confirm it disappears cleanly without disturbing unrelated schedules.' \
		'Create a near-future relay schedule, issue a manual relay command before the due minute, and confirm the next scheduled run still executes normally.' \
		'Wait for the due minute and confirm the physical relay, GET /api/status, and scheduler last-result or next-run fields all agree that execution came from the scheduler path.' \
		'Reboot the device before one scheduled minute passes, then reopen the scheduler view or GET /api/schedules and confirm edited, disabled, enabled, and deleted schedule state is still correct.' \
		'Restore trusted time after that reboot and confirm the missed run is skipped rather than replayed automatically.' \
		'Force or simulate a major clock correction and confirm the scheduler recomputes only future runs without backfilling earlier missed minutes.' \
		'Attempt to save two enabled schedules that command opposite relay states in the same minute and confirm the save is rejected with operator-visible feedback.' \
		'Confirm recent scheduler problems remain visible after rejected saves, untrusted time, or degraded-time scenarios.' \
		'Record which scheduler gating, reset-then-degrade, create, edit, enable, disable, delete, manual-versus-scheduled, scheduled execution, reboot persistence, missed-job skip, time-correction recompute, conflict rejection, and recent-problem visibility scenarios passed or failed.'
}

print_phase8_ready_state_markers() {
	print_phase7_ready_state_markers
	printf '%s\n' \
		'OTA runtime ready current=... confirmed=... state=... remote=lukasa1993/L-Controller every=...h' \
		'OTA staged version=...' \
		'OTA last_attempt=... version=... rollback=... error=... bytes=...'
}

print_phase8_curl_commands() {
	printf '%s\n' \
		"curl -i http://<device-ip>/api/status" \
		"curl -i -c cookie.txt -H 'Content-Type: application/json' -d '{\"username\":\"...\",\"password\":\"...\"}' http://<device-ip>/api/auth/login" \
		"curl -i -b cookie.txt http://<device-ip>/api/update" \
		"curl -i -b cookie.txt -H 'Content-Type: application/octet-stream' --data-binary @build/nrf7002dk_nrf5340_cpuapp/app/zephyr/zephyr.signed.bin http://<device-ip>/api/update/upload" \
		"curl -i -b cookie.txt -X POST http://<device-ip>/api/update/apply" \
		"curl -i -b cookie.txt -X POST http://<device-ip>/api/update/clear" \
		"curl -i -b cookie.txt -X POST http://<device-ip>/api/update/remote-check" \
		"curl -i -b cookie.txt http://<device-ip>/api/status"
}

print_phase8_scenario_labels() {
	printf '%s\n' \
		'local upload staging' \
		'same-version rejection' \
		'downgrade rejection' \
		'explicit apply reboot' \
		'post-boot confirmation' \
		'rollback visibility' \
		'github update now' \
		'daily retry' \
		'panel truthfulness'
}

print_phase8_device_checklist() {
	printf '%s\n' \
		'Re-run ./scripts/validate.sh first so the canonical automated sysbuild path is green.' \
		'Flash the latest firmware with ./scripts/flash.sh.' \
		'Open the device console with ./scripts/console.sh, determine the device IP from the boot log, and confirm the ready-state, relay, scheduler, and OTA markers appear.' \
		'Log in to the panel and confirm the dedicated update surface shows current version, staged version state, last result, and pending-update warnings truthfully before testing.' \
		'Upload a newer signed firmware image locally and confirm it stages successfully without rebooting until the explicit apply action is used.' \
		'Attempt one same-version upload and one older-version upload, and confirm both are rejected with clear operator feedback and no unsafe staged image remains.' \
		'Trigger apply for the newer staged image and confirm the browser disconnects during reboot, then requires a fresh login after the device returns.' \
		'After reboot, confirm the new firmware reaches APP_READY, stays healthy through the stable window, and only then becomes confirmed.' \
		'Exercise one rollback path by preventing confirmation or using a known-bad image, then confirm the device returns to the previous firmware and the next boot surfaces rollback or failed-attempt status clearly.' \
		'Use the explicit Update now action while upstream internet is available and confirm the device checks GitHub Releases, selects the latest stable eligible artifact, and stages or applies it through the same OTA safety rules.' \
		'Force or simulate one failed GitHub update cycle, then allow the next daily check window and confirm the device retries instead of pausing remote updates permanently.' \
		'Refresh the panel or API before staging, after staging, after apply, and after confirmation or rollback, and confirm current version, staged or last-attempted version, last result, rollback flags, and remote busy state remain internally consistent.' \
		'Record which local upload staging, same-version rejection, downgrade rejection, explicit apply reboot, post-boot confirmation, rollback visibility, GitHub update-now, daily retry, and panel truthfulness scenarios passed or failed.'
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
