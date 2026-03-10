#include <errno.h>

#include <zephyr/logging/log.h>
#include <zephyr/net/net_if.h>

#include "app/bootstrap.h"
#include "network/network_supervisor.h"
#include "network/wifi_lifecycle.h"
#include "panel/panel_auth.h"
#include "panel/panel_http.h"
#include "persistence/persistence.h"
#include "recovery/recovery.h"
#include "relay/relay.h"

LOG_MODULE_DECLARE(app, CONFIG_LOG_DEFAULT_LEVEL);

static void log_preserved_recovery_reset(const struct app_context *app_context)
{
	const struct recovery_manager *manager = &app_context->recovery;
	const struct recovery_reset_cause *reset_cause = recovery_manager_last_reset_cause(manager);

	if (reset_cause == NULL || !reset_cause->available) {
		return;
	}

	LOG_WRN("Previous recovery reset=%s hw=0x%x state=%s stage=%s reason=%d",
		recovery_manager_reset_trigger_text(reset_cause->trigger),
		reset_cause->hardware_reset_cause,
		network_supervisor_connectivity_state_text(reset_cause->connectivity_state),
		network_supervisor_failure_stage_text(reset_cause->failure_stage),
		reset_cause->reason);
	LOG_WRN("Recovery cooldown=%dms stable_window=%dms",
		app_context->config.recovery.cooldown_ms,
		app_context->config.recovery.stable_window_ms);
}

static bool persistence_status_requires_warning(
	const struct persistence_section_status *status)
{
	return status->reseeded ||
	       status->state == PERSISTENCE_LOAD_STATE_INVALID_RESET ||
	       status->state == PERSISTENCE_LOAD_STATE_INCOMPATIBLE_RESET;
}

static void log_persistence_section_status(
	const struct persistence_section_status *status)
{
	const char *section_text;
	const char *state_text;
	const char *migration_text;
	const char *suffix = "";

	if (status == NULL) {
		return;
	}

	section_text = persistence_section_text(status->section);
	state_text = persistence_load_state_text(status->state);
	migration_text = persistence_migration_action_text(status->migration_action);
	if (status->reseeded) {
		suffix = status->section == PERSISTENCE_SECTION_AUTH ?
			" (reseeded configured credentials)" : " (reseeded)";
	}

	if (status->state == PERSISTENCE_LOAD_STATE_INCOMPATIBLE_RESET) {
		LOG_WRN("Persistence %s: %s%s (stored schema v%u -> layout v%u, migration=%s)",
			section_text,
			state_text,
			suffix,
			status->stored_schema_version,
			status->expected_schema_version,
			migration_text);
		return;
	}

	if (status->state == PERSISTENCE_LOAD_STATE_INVALID_RESET) {
		LOG_WRN("Persistence %s: %s%s (corrupt section reset safely)",
			section_text,
			state_text,
			suffix);
		return;
	}

	if (persistence_status_requires_warning(status)) {
		LOG_WRN("Persistence %s: %s%s", section_text, state_text, suffix);
		return;
	}

	LOG_INF("Persistence %s: %s", section_text, state_text);
}

static void log_persistence_load_report(
	const struct persistence_load_report *load_report)
{
	if (load_report == NULL) {
		return;
	}

	log_persistence_section_status(&load_report->auth);
	log_persistence_section_status(&load_report->actions);
	log_persistence_section_status(&load_report->relay);
	log_persistence_section_status(&load_report->schedule);
}

static const char *relay_state_text(bool relay_state)
{
	return relay_state ? "on" : "off";
}

static const char *relay_safety_note_text(const char *safety_note)
{
	return safety_note != NULL ? safety_note : "none";
}

static void log_persisted_snapshot_summary(const struct persisted_config *config)
{
	if (config == NULL) {
		return;
	}

	LOG_INF("Persistence snapshot ready (layout v%u, actions=%u, schedules=%u, relay desired=%s, relay reboot=%s)",
		config->layout_version,
		config->actions.count,
		config->schedule.count,
		relay_state_text(config->relay.last_desired_state),
		persisted_relay_reboot_policy_text(config->relay.reboot_policy));
}

static void log_relay_runtime_status(const struct app_context *app_context)
{
	const struct relay_runtime_status *status;

	status = relay_service_get_status(&app_context->relay);
	if (status == NULL || !status->implemented || !status->available) {
		return;
	}

	LOG_INF("Relay runtime ready actual=%s desired=%s source=%s note=%s",
		relay_state_text(status->actual_state),
		relay_state_text(status->desired_state),
		relay_status_source_text(status->source),
		relay_safety_note_text(status->safety_note));
}

static void log_scheduler_runtime_status(const struct app_context *app_context)
{
	const struct scheduler_runtime_status *status;

	status = scheduler_service_get_status(&app_context->scheduler);
	if (status == NULL || !status->implemented) {
		return;
	}

	LOG_INF("Scheduler runtime ready schedules=%u enabled=%u automation=%s clock=%s degraded=%s UTC cadence=%us timeout=%dms",
		status->schedule_count,
		status->enabled_schedule_count,
		status->automation_active ? "active" : "inactive",
		scheduler_clock_trust_state_text(status->clock_state),
		scheduler_degraded_reason_text(status->degraded_reason),
		app_context->config.scheduler.cadence_seconds,
		app_context->config.scheduler.trusted_clock_timeout_ms);

	if (status->next_run.available) {
		LOG_INF("Scheduler next_run minute=%lld schedule=%s action=%s",
			(long long)status->next_run.normalized_utc_minute,
			status->next_run.schedule_id,
			status->next_run.action_id);
	} else {
		LOG_INF("Scheduler boot contract keeps automation inactive until trusted UTC time baselines the current minute, skip missed work, and calculates only future run candidates");
	}

	if (status->last_result.available) {
		LOG_INF("Scheduler last_result=%s minute=%lld",
			scheduler_last_result_code_text(status->last_result.code),
			(long long)status->last_result.normalized_utc_minute);
	}
}

static int load_app_config(struct app_context *app_context)
{
	int ret;

	app_context->config = (struct app_config){
		.board_name = CONFIG_BOARD,
		.wifi = {
			.ssid = CONFIG_APP_WIFI_SSID,
			.psk = CONFIG_APP_WIFI_PSK,
			.timeout_ms = CONFIG_APP_WIFI_TIMEOUT_MS,
			.retry_interval_ms = CONFIG_APP_WIFI_RETRY_INTERVAL_MS,
		},
		.reachability = {
			.host = CONFIG_APP_REACHABILITY_HOST,
			.port = 80,
		},
		.recovery = {
			.watchdog_timeout_ms = CONFIG_APP_RECOVERY_WATCHDOG_TIMEOUT_MS,
			.degraded_patience_ms = CONFIG_APP_RECOVERY_DEGRADED_PATIENCE_MS,
			.stable_window_ms = CONFIG_APP_RECOVERY_STABLE_WINDOW_MS,
			.cooldown_ms = CONFIG_APP_RECOVERY_COOLDOWN_MS,
		},
		.panel = {
			.port = APP_PANEL_PORT,
			.max_sessions = APP_PANEL_MAX_SESSIONS,
			.login_failure_limit = APP_PANEL_LOGIN_FAILURE_LIMIT,
			.login_cooldown_ms = APP_PANEL_LOGIN_COOLDOWN_MS,
		},
		.persistence = {
			.layout_version = APP_PERSISTENCE_LAYOUT_VERSION,
			.default_relay_reboot_policy = APP_RELAY_REBOOT_POLICY_DEFAULT,
		},
		.scheduler = {
			.cadence_seconds = APP_SCHEDULER_CADENCE_SECONDS,
			.trusted_clock_timeout_ms =
				APP_SCHEDULER_TRUSTED_CLOCK_TIMEOUT_MS,
			.problem_history_capacity =
				APP_SCHEDULER_PROBLEM_HISTORY_CAPACITY,
		},
	};

	ret = wifi_lifecycle_security_from_text(CONFIG_APP_WIFI_SECURITY,
					       &app_context->config.wifi.security);
	if (ret != 0) {
		LOG_ERR("Unsupported CONFIG_APP_WIFI_SECURITY value: %s", CONFIG_APP_WIFI_SECURITY);
		return ret;
	}

	return 0;
}

static int load_persisted_config(struct app_context *app_context)
{
	int ret;

	ret = persistence_store_init(&app_context->persistence, &app_context->config);
	if (ret != 0) {
		LOG_ERR("Failed to initialize persistence store: %d", ret);
		return ret;
	}

	ret = persistence_store_load(&app_context->persistence,
				     &app_context->persisted_config);
	if (ret != 0) {
		LOG_ERR("Failed to load persisted configuration: %d", ret);
		return ret;
	}

	log_persisted_snapshot_summary(&app_context->persisted_config);
	log_persistence_load_report(&app_context->persisted_config.load_report);

	return 0;
}

int app_boot(struct app_context *app_context)
{
	struct network_supervisor_status network_status;
	struct net_if *wifi_iface;
	int ret;

	if (app_context == NULL) {
		return -EINVAL;
	}

	ret = load_app_config(app_context);
	if (ret != 0) {
		return ret;
	}

	ret = load_persisted_config(app_context);
	if (ret != 0) {
		return ret;
	}

	ret = recovery_manager_init(&app_context->recovery, app_context);
	if (ret != 0) {
		return ret;
	}

	log_preserved_recovery_reset(app_context);

	ret = relay_service_init(&app_context->relay, app_context);
	if (ret != 0) {
		LOG_ERR("Failed to initialize relay service: %d", ret);
		return ret;
	}

	ret = action_dispatcher_init(&app_context->actions, app_context);
	if (ret != 0) {
		LOG_ERR("Failed to initialize action dispatcher: %d", ret);
		return ret;
	}

	ret = scheduler_service_init(&app_context->scheduler, app_context);
	if (ret != 0) {
		LOG_ERR("Failed to initialize scheduler service: %d", ret);
		return ret;
	}

	log_relay_runtime_status(app_context);
	log_scheduler_runtime_status(app_context);

	ret = panel_auth_service_init(&app_context->panel_auth, app_context);
	if (ret != 0) {
		LOG_ERR("Failed to initialize panel auth service: %d", ret);
		return ret;
	}

	wifi_iface = net_if_get_first_wifi();
	if (wifi_iface == NULL) {
		LOG_ERR("No Wi-Fi interface found");
		return -ENODEV;
	}

	network_supervisor_init(&app_context->network_state, wifi_iface, &app_context->recovery);
	recovery_manager_startup_begin(&app_context->recovery);

	LOG_INF("Booting Wi-Fi bring-up app on %s", app_context->config.board_name);

	ret = network_supervisor_start(&app_context->network_state, &app_context->config);
	if (ret != 0) {
		if (network_supervisor_get_status(&app_context->network_state, &network_status) == 0 &&
		    network_status.last_failure.recorded) {
			LOG_ERR("Network supervisor failed during %s: %d",
				network_supervisor_failure_stage_text(
					network_status.last_failure.failure_stage),
				network_status.last_failure.reason);
		}

		return ret;
	}

	recovery_manager_startup_complete(&app_context->recovery);

	ret = network_supervisor_get_status(&app_context->network_state, &network_status);
	if (ret == 0) {
		LOG_INF("Network supervisor entered %s",
			network_supervisor_connectivity_state_text(
				network_status.connectivity_state));

		if (network_status.connectivity_state != NETWORK_CONNECTIVITY_HEALTHY) {
			LOG_WRN("Continuing boot with background network recovery in %s",
				network_supervisor_connectivity_state_text(
					network_status.connectivity_state));
		}

		if (network_status.last_failure.recorded) {
			LOG_WRN("Most recent network failure during %s: %d",
				network_supervisor_failure_stage_text(
					network_status.last_failure.failure_stage),
				network_status.last_failure.reason);
		}
	}

	ret = panel_http_server_init(&app_context->panel_http, app_context);
	if (ret != 0) {
		LOG_ERR("Failed to initialize panel HTTP shell: %d", ret);
		return ret;
	}

	LOG_INF("APP_READY");
	return 0;
}
