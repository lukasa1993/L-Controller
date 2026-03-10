#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <zephyr/net/net_ip.h>
#include <zephyr/sys/util.h>

#include "actions/actions.h"
#include "app/app_context.h"
#include "network/network_supervisor.h"
#include "panel/panel_status.h"
#include "persistence/persistence.h"
#include "recovery/recovery.h"

static const char *panel_status_json_bool(bool value)
{
	return value ? "true" : "false";
}

static int panel_status_append(char *buffer,
				      size_t buffer_len,
				      size_t *offset,
				      const char *format,
				      ...)
{
	va_list args;
	int written;

	if (buffer == NULL || offset == NULL || format == NULL || *offset >= buffer_len) {
		return -EINVAL;
	}

	va_start(args, format);
	written = vsnprintf(buffer + *offset, buffer_len - *offset, format, args);
	va_end(args);
	if (written < 0 || (size_t)written >= buffer_len - *offset) {
		return -ENOMEM;
	}

	*offset += (size_t)written;
	return 0;
}

static const char *panel_status_scheduler_action_label(const char *action_id)
{
	const char *label = action_dispatcher_public_action_label(action_id);

	if (action_id == NULL || action_id[0] == '\0') {
		return "none";
	}

	return label != NULL ? label : "Unknown Action";
}

static const char *panel_status_scheduler_action_key(const char *action_id)
{
	const char *key = action_dispatcher_public_action_key(action_id);

	return key != NULL ? key : "none";
}

static const char *panel_status_relay_note_text(const char *note)
{
	return note != NULL ? note : "none";
}

static bool panel_status_relay_blocked(const struct relay_runtime_status *status)
{
	return status == NULL || !status->implemented || !status->available;
}

static void panel_status_format_ota_version(const struct persisted_ota_version *version,
					    char *buffer,
					    size_t buffer_len)
{
	if (buffer == NULL || buffer_len == 0U) {
		return;
	}

	if (version == NULL || !version->available) {
		snprintf(buffer, buffer_len, "Unavailable");
		return;
	}

	snprintf(buffer,
		 buffer_len,
		 "%u.%u.%u+%u",
		 version->major,
		 version->minor,
		 version->revision,
		 version->build_num);
}

static const char *panel_status_ota_pending_warning(const struct ota_runtime_status *status)
{
	if (status == NULL) {
		return "Firmware update status is unavailable.";
	}

	switch (status->state) {
	case PERSISTED_OTA_STATE_STAGING:
		return "Upload staging is in progress.";
	case PERSISTED_OTA_STATE_STAGED:
		return "A staged firmware image is waiting for explicit apply.";
	case PERSISTED_OTA_STATE_APPLY_REQUESTED:
		return "A staged firmware image has been queued for reboot.";
	case PERSISTED_OTA_STATE_IDLE:
	default:
		return "No staged firmware image is waiting.";
	}
}

static int panel_status_render_update_json(struct app_context *app_context,
					   char *buffer,
					   size_t buffer_len)
{
	struct ota_runtime_status ota_status;
	char current_version[24];
	char staged_version[24];
	size_t offset = 0U;
	int ret;

	if (app_context == NULL || buffer == NULL || buffer_len == 0U) {
		return -EINVAL;
	}

	ret = ota_service_copy_snapshot(&app_context->ota, &ota_status);
	if (ret != 0) {
		return ret;
	}

	panel_status_format_ota_version(&ota_status.current_version,
					 current_version,
					 sizeof(current_version));
	panel_status_format_ota_version(&ota_status.staged_version,
					 staged_version,
					 sizeof(staged_version));

	return panel_status_append(
		buffer,
		buffer_len,
		&offset,
		"{"
		"\"implemented\":%s,"
		"\"state\":\"%s\","
		"\"imageConfirmed\":%s,"
		"\"currentVersion\":\"%s\","
		"\"currentVersionAvailable\":%s,"
		"\"stagedVersion\":\"%s\","
		"\"stagedVersionAvailable\":%s,"
		"\"lastResult\":\"%s\","
		"\"lastResultRecorded\":%s,"
		"\"rollback\":{\"detected\":%s,\"reason\":%d},"
		"\"applyReady\":%s,"
		"\"pendingWarning\":\"%s\""
		"}",
		panel_status_json_bool(ota_status.implemented),
		persistence_ota_state_text(ota_status.state),
		panel_status_json_bool(ota_status.image_confirmed),
		current_version,
		panel_status_json_bool(ota_status.current_version.available),
		staged_version,
		panel_status_json_bool(ota_status.staged_version.available),
		persistence_ota_last_result_text(ota_status.last_attempt.result),
		panel_status_json_bool(ota_status.last_attempt.recorded),
		panel_status_json_bool(ota_status.last_attempt.rollback_detected),
		ota_status.last_attempt.rollback_reason,
		panel_status_json_bool(ota_status.state == PERSISTED_OTA_STATE_STAGED &&
				       ota_status.staged_version.available),
		panel_status_ota_pending_warning(&ota_status));
}

static const char *panel_status_relay_blocked_reason(const struct relay_runtime_status *status)
{
	if (!panel_status_relay_blocked(status)) {
		return "none";
	}

	if (status == NULL || !status->implemented) {
		return "Relay control is not implemented on this build.";
	}

	return "Relay runtime is unavailable.";
}

static int panel_status_render_scheduler_json(struct app_context *app_context,
					      char *buffer,
					      size_t buffer_len)
{
	struct scheduler_runtime_status scheduler_status;
	struct scheduler_problem_record problems[SCHEDULER_PROBLEM_HISTORY_CAPACITY];
	uint32_t copied_problem_count = 0U;
	size_t offset = 0U;
	uint32_t index;
	int ret;

	ret = scheduler_service_copy_snapshot(&app_context->scheduler,
					     &scheduler_status,
					     problems,
					     ARRAY_SIZE(problems),
					     &copied_problem_count);
	if (ret != 0) {
		return ret;
	}

	ret = panel_status_append(
		buffer,
		buffer_len,
		&offset,
		"{"
		"\"implemented\":%s,"
		"\"utcOnly\":%s,"
		"\"minuteResolutionOnly\":%s,"
		"\"clockTrusted\":%s,"
		"\"automationActive\":%s,"
		"\"clockState\":\"%s\","
		"\"degradedReason\":\"%s\","
		"\"scheduleCount\":%u,"
		"\"enabledCount\":%u,"
		"\"nextRun\":{"
		"\"available\":%s,"
		"\"normalizedUtcMinute\":%lld,"
		"\"scheduleId\":\"%s\","
		"\"actionKey\":\"%s\","
		"\"actionLabel\":\"%s\""
		"},"
		"\"lastResult\":{"
		"\"available\":%s,"
		"\"code\":\"%s\","
		"\"errorCode\":%d,"
		"\"normalizedUtcMinute\":%lld,"
		"\"scheduleId\":\"%s\","
		"\"actionKey\":\"%s\","
		"\"actionLabel\":\"%s\""
		"},"
		"\"problemCount\":%u,"
		"\"problems\":[",
		panel_status_json_bool(scheduler_status.implemented),
		panel_status_json_bool(scheduler_status.utc_only),
		panel_status_json_bool(scheduler_status.minute_resolution_only),
		panel_status_json_bool(scheduler_status.clock_state ==
				      SCHEDULER_CLOCK_TRUST_STATE_TRUSTED),
		panel_status_json_bool(scheduler_status.automation_active),
		scheduler_clock_trust_state_text(scheduler_status.clock_state),
		scheduler_degraded_reason_text(scheduler_status.degraded_reason),
		scheduler_status.schedule_count,
		scheduler_status.enabled_schedule_count,
		panel_status_json_bool(scheduler_status.next_run.available),
		(long long)scheduler_status.next_run.normalized_utc_minute,
		scheduler_status.next_run.schedule_id,
		panel_status_scheduler_action_key(scheduler_status.next_run.action_id),
		panel_status_scheduler_action_label(scheduler_status.next_run.action_id),
		panel_status_json_bool(scheduler_status.last_result.available),
		scheduler_last_result_code_text(scheduler_status.last_result.code),
		scheduler_status.last_result.error_code,
		(long long)scheduler_status.last_result.normalized_utc_minute,
		scheduler_status.last_result.schedule_id,
		panel_status_scheduler_action_key(scheduler_status.last_result.action_id),
		panel_status_scheduler_action_label(scheduler_status.last_result.action_id),
		scheduler_status.problem_count);
	if (ret != 0) {
		return ret;
	}

	for (index = 0U; index < copied_problem_count; ++index) {
		ret = panel_status_append(
			buffer,
			buffer_len,
			&offset,
			"%s{"
			"\"code\":\"%s\","
			"\"errorCode\":%d,"
			"\"normalizedUtcMinute\":%lld,"
			"\"scheduleId\":\"%s\","
			"\"actionKey\":\"%s\","
			"\"actionLabel\":\"%s\""
			"}",
			index == 0U ? "" : ",",
			scheduler_problem_code_text(problems[index].code),
			problems[index].error_code,
			(long long)problems[index].normalized_utc_minute,
			problems[index].schedule_id,
			panel_status_scheduler_action_key(problems[index].action_id),
			panel_status_scheduler_action_label(problems[index].action_id));
		if (ret != 0) {
			return ret;
		}
	}

	return panel_status_append(
		buffer,
		buffer_len,
		&offset,
		"],"
		"\"placeholder\":\"Schedule management is now available from the dedicated scheduler surface.\""
		"}");
}

int panel_status_render_schedule_snapshot_json(struct app_context *app_context,
					       char *buffer,
					       size_t buffer_len)
{
	struct scheduler_runtime_status scheduler_status;
	struct scheduler_problem_record problems[SCHEDULER_PROBLEM_HISTORY_CAPACITY];
	const struct persisted_schedule_table *schedule_table;
	uint32_t copied_problem_count = 0U;
	uint32_t copied_schedule_count;
	size_t offset = 0U;
	uint32_t index;
	int ret;

	if (app_context == NULL || buffer == NULL || buffer_len == 0U) {
		return -EINVAL;
	}

	ret = scheduler_service_copy_snapshot(&app_context->scheduler,
					     &scheduler_status,
					     problems,
					     ARRAY_SIZE(problems),
					     &copied_problem_count);
	if (ret != 0) {
		return ret;
	}

	schedule_table = &app_context->persisted_config.schedule;
	copied_schedule_count = MIN(schedule_table->count, PERSISTED_SCHEDULE_MAX_COUNT);

	ret = panel_status_append(
		buffer,
		buffer_len,
		&offset,
		"{"
		"\"implemented\":%s,"
		"\"timezone\":\"UTC\","
		"\"utcOnly\":%s,"
		"\"minuteResolutionOnly\":%s,"
		"\"clockTrusted\":%s,"
		"\"automationActive\":%s,"
		"\"clockState\":\"%s\","
		"\"degradedReason\":\"%s\","
		"\"scheduleCount\":%u,"
		"\"enabledCount\":%u,"
		"\"maxSchedules\":%u,"
		"\"problemHistoryCapacity\":%u,"
		"\"nextRun\":{"
		"\"available\":%s,"
		"\"normalizedUtcMinute\":%lld,"
		"\"scheduleId\":\"%s\","
		"\"actionKey\":\"%s\","
		"\"actionLabel\":\"%s\""
		"},"
		"\"lastResult\":{"
		"\"available\":%s,"
		"\"code\":\"%s\","
		"\"errorCode\":%d,"
		"\"normalizedUtcMinute\":%lld,"
		"\"scheduleId\":\"%s\","
		"\"actionKey\":\"%s\","
		"\"actionLabel\":\"%s\""
		"},"
		"\"problemCount\":%u,"
		"\"actionChoices\":["
		"{\"key\":\"relay-on\",\"label\":\"Relay On\"},"
		"{\"key\":\"relay-off\",\"label\":\"Relay Off\"}"
		"],"
		"\"problems\":[",
		panel_status_json_bool(scheduler_status.implemented),
		panel_status_json_bool(scheduler_status.utc_only),
		panel_status_json_bool(scheduler_status.minute_resolution_only),
		panel_status_json_bool(scheduler_status.clock_state ==
				      SCHEDULER_CLOCK_TRUST_STATE_TRUSTED),
		panel_status_json_bool(scheduler_status.automation_active),
		scheduler_clock_trust_state_text(scheduler_status.clock_state),
		scheduler_degraded_reason_text(scheduler_status.degraded_reason),
		scheduler_status.schedule_count,
		scheduler_status.enabled_schedule_count,
		PERSISTED_SCHEDULE_MAX_COUNT,
		SCHEDULER_PROBLEM_HISTORY_CAPACITY,
		panel_status_json_bool(scheduler_status.next_run.available),
		(long long)scheduler_status.next_run.normalized_utc_minute,
		scheduler_status.next_run.schedule_id,
		panel_status_scheduler_action_key(scheduler_status.next_run.action_id),
		panel_status_scheduler_action_label(scheduler_status.next_run.action_id),
		panel_status_json_bool(scheduler_status.last_result.available),
		scheduler_last_result_code_text(scheduler_status.last_result.code),
		scheduler_status.last_result.error_code,
		(long long)scheduler_status.last_result.normalized_utc_minute,
		scheduler_status.last_result.schedule_id,
		panel_status_scheduler_action_key(scheduler_status.last_result.action_id),
		panel_status_scheduler_action_label(scheduler_status.last_result.action_id),
		scheduler_status.problem_count);
	if (ret != 0) {
		return ret;
	}

	for (index = 0U; index < copied_problem_count; ++index) {
		ret = panel_status_append(
			buffer,
			buffer_len,
			&offset,
			"%s{"
			"\"code\":\"%s\","
			"\"errorCode\":%d,"
			"\"normalizedUtcMinute\":%lld,"
			"\"scheduleId\":\"%s\","
			"\"actionKey\":\"%s\","
			"\"actionLabel\":\"%s\""
			"}",
			index == 0U ? "" : ",",
			scheduler_problem_code_text(problems[index].code),
			problems[index].error_code,
			(long long)problems[index].normalized_utc_minute,
			problems[index].schedule_id,
			panel_status_scheduler_action_key(problems[index].action_id),
			panel_status_scheduler_action_label(problems[index].action_id));
		if (ret != 0) {
			return ret;
		}
	}

	ret = panel_status_append(buffer, buffer_len, &offset, "],\"schedules\":[");
	if (ret != 0) {
		return ret;
	}

	for (index = 0U; index < copied_schedule_count; ++index) {
		const struct persisted_schedule *schedule = &schedule_table->entries[index];
		const bool is_next_run = scheduler_status.next_run.available &&
			strcmp(schedule->schedule_id, scheduler_status.next_run.schedule_id) == 0;

		ret = panel_status_append(
			buffer,
			buffer_len,
			&offset,
			"%s{"
			"\"scheduleId\":\"%s\","
			"\"enabled\":%s,"
			"\"cronExpression\":\"%s\","
			"\"actionKey\":\"%s\","
			"\"actionLabel\":\"%s\","
			"\"isNextRun\":%s"
			"}",
			index == 0U ? "" : ",",
			schedule->schedule_id,
			panel_status_json_bool(schedule->enabled),
			schedule->cron_expression,
			panel_status_scheduler_action_key(schedule->action_id),
			panel_status_scheduler_action_label(schedule->action_id),
			panel_status_json_bool(is_next_run));
		if (ret != 0) {
			return ret;
		}
	}

	return panel_status_append(buffer, buffer_len, &offset, "]}");
}

int panel_status_render_json(struct app_context *app_context,
			     char *buffer,
			     size_t buffer_len)
{
	struct network_supervisor_status network_status;
	const struct relay_runtime_status *relay_status;
	const struct recovery_reset_cause *reset_cause;
	const char *relay_source = "none";
	const char *relay_safety_note = "none";
	const char *relay_blocked_reason = "none";
	char ipv4_address[NET_IPV4_ADDR_LEN] = "";
	char scheduler_json[4096] = "";
	char update_json[1024] = "";
	const char *reset_trigger = "none";
	const char *reset_failure_stage = "none";
	const char *reset_connectivity = "not-ready";
	bool relay_available = false;
	bool relay_implemented = false;
	bool relay_actual_state = false;
	bool relay_desired_state = false;
	bool relay_pending = false;
	bool relay_blocked = false;
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

	ret = panel_status_render_scheduler_json(app_context,
					 scheduler_json,
					 sizeof(scheduler_json));
	if (ret != 0) {
		return ret;
	}

	ret = panel_status_render_update_json(app_context,
					      update_json,
					      sizeof(update_json));
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

	relay_status = relay_service_get_status(&app_context->relay);
	if (relay_status != NULL) {
		relay_implemented = relay_status->implemented;
		relay_available = relay_status->available;
		relay_actual_state = relay_status->actual_state;
		relay_desired_state = relay_status->desired_state;
		relay_source = relay_status_source_text(relay_status->source);
		relay_safety_note = panel_status_relay_note_text(relay_status->safety_note);
		relay_blocked = panel_status_relay_blocked(relay_status);
		relay_blocked_reason = panel_status_relay_blocked_reason(relay_status);
	}

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
		"\"implemented\":%s,\"available\":%s,\"actualState\":%s,"
		"\"desiredState\":%s,\"source\":\"%s\",\"safetyNote\":\"%s\","
		"\"pending\":%s,\"pendingReason\":\"none\",\"blocked\":%s,"
		"\"blockedReason\":\"%s\",\"rebootPolicy\":\"%s\""
		"},"
		"\"scheduler\":%s,"
		"\"update\":%s"
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
		panel_status_json_bool(relay_implemented),
		panel_status_json_bool(relay_available),
		panel_status_json_bool(relay_actual_state),
		panel_status_json_bool(relay_desired_state),
		relay_source,
		relay_safety_note,
		panel_status_json_bool(relay_pending),
		panel_status_json_bool(relay_blocked),
		relay_blocked_reason,
		persisted_relay_reboot_policy_text(app_context->persisted_config.relay.reboot_policy),
		scheduler_json,
		update_json);
	if (written < 0 || (size_t)written >= buffer_len) {
		return -ENOMEM;
	}

	return written;
}
