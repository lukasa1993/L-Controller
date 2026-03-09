#include <errno.h>
#include <stdio.h>

#include <zephyr/net/net_ip.h>

#include "app/app_context.h"
#include "network/network_supervisor.h"
#include "panel/panel_status.h"
#include "persistence/persistence.h"
#include "recovery/recovery.h"

static const char *panel_status_json_bool(bool value)
{
	return value ? "true" : "false";
}

static uint32_t panel_status_enabled_schedule_count(const struct persisted_schedule_table *schedule)
{
	uint32_t enabled_count = 0;

	for (uint32_t index = 0; index < schedule->count && index < PERSISTED_SCHEDULE_MAX_COUNT; ++index) {
		if (schedule->entries[index].enabled) {
			enabled_count++;
		}
	}

	return enabled_count;
}

int panel_status_render_json(struct app_context *app_context,
			     char *buffer,
			     size_t buffer_len)
{
	struct network_supervisor_status network_status;
	const struct recovery_reset_cause *reset_cause;
	char ipv4_address[NET_IPV4_ADDR_LEN] = "";
	uint32_t enabled_schedule_count;
	const char *reset_trigger = "none";
	const char *reset_failure_stage = "none";
	const char *reset_connectivity = "not-ready";
	bool reset_available = false;
	bool recovery_reset = false;
	uint32_t hardware_reset_cause = 0;
	int reset_reason = 0;
	int written;
	int ret;

	if (app_context == NULL || buffer == NULL || buffer_len == 0) {
		return -EINVAL;
	}

	ret = network_supervisor_get_status(&app_context->network_state, &network_status);
	if (ret != 0) {
		return ret;
	}

	if (network_status.ipv4_bound) {
		(void)net_addr_ntop(AF_INET,
				   &network_status.leased_ipv4,
				   ipv4_address,
				   sizeof(ipv4_address));
	}

	reset_cause = recovery_manager_last_reset_cause(&app_context->recovery);
	if (reset_cause != NULL && reset_cause->available) {
		reset_available = true;
		recovery_reset = reset_cause->recovery_reset;
		hardware_reset_cause = reset_cause->hardware_reset_cause;
		reset_trigger = recovery_manager_reset_trigger_text(reset_cause->trigger);
		reset_failure_stage =
			network_supervisor_failure_stage_text(reset_cause->failure_stage);
		reset_connectivity =
			network_supervisor_connectivity_state_text(reset_cause->connectivity_state);
		reset_reason = reset_cause->reason;
	}

	enabled_schedule_count =
		panel_status_enabled_schedule_count(&app_context->persisted_config.schedule);

	written = snprintf(
		buffer,
		buffer_len,
		"{"
		"\"device\":{\"board\":\"%s\",\"ready\":true,\"panelPort\":%u},"
		"\"network\":{"
		"\"connectivity\":\"%s\",\"wifiReady\":%s,\"wifiConnected\":%s,"
		"\"ipv4Bound\":%s,\"ipv4Address\":\"%s\",\"reachabilityOk\":%s,"
		"\"lastReachabilityStatus\":%d,"
		"\"lastFailure\":{\"recorded\":%s,\"stage\":\"%s\",\"reason\":%d}"
		"},"
		"\"recovery\":{"
		"\"available\":%s,\"recoveryReset\":%s,\"hardwareResetCause\":%u,"
		"\"trigger\":\"%s\",\"failureStage\":\"%s\",\"reason\":%d,"
		"\"connectivity\":\"%s\""
		"},"
		"\"relay\":{"
		"\"implemented\":false,\"configuredActions\":%u,\"lastDesiredState\":%s,"
		"\"rebootPolicy\":\"%s\","
		"\"placeholder\":\"Relay controls arrive in Phase 6.\""
		"},"
		"\"scheduler\":{"
		"\"implemented\":false,\"scheduleCount\":%u,\"enabledCount\":%u,"
		"\"placeholder\":\"Scheduling arrives in Phase 7.\""
		"},"
		"\"update\":{"
		"\"implemented\":false,"
		"\"placeholder\":\"Firmware update workflows arrive in Phase 8.\""
		"}"
		"}",
		app_context->config.board_name,
		app_context->config.panel.port,
		network_supervisor_connectivity_state_text(network_status.connectivity_state),
		panel_status_json_bool(network_status.wifi_ready),
		panel_status_json_bool(network_status.wifi_connected),
		panel_status_json_bool(network_status.ipv4_bound),
		ipv4_address,
		panel_status_json_bool(network_status.reachability_ok),
		network_status.last_reachability_status,
		panel_status_json_bool(network_status.last_failure.recorded),
		network_supervisor_failure_stage_text(network_status.last_failure.failure_stage),
		network_status.last_failure.reason,
		panel_status_json_bool(reset_available),
		panel_status_json_bool(recovery_reset),
		hardware_reset_cause,
		reset_trigger,
		reset_failure_stage,
		reset_reason,
		reset_connectivity,
		app_context->persisted_config.actions.count,
		panel_status_json_bool(app_context->persisted_config.relay.last_desired_state),
		persisted_relay_reboot_policy_text(app_context->persisted_config.relay.reboot_policy),
		app_context->persisted_config.schedule.count,
		enabled_schedule_count);
	if (written < 0 || (size_t)written >= buffer_len) {
		return -ENOMEM;
	}

	return written;
}
