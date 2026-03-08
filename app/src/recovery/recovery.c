#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/sys/util.h>

#include "app/app_context.h"
#include "recovery/recovery.h"

LOG_MODULE_DECLARE(app, CONFIG_LOG_DEFAULT_LEVEL);

#if DT_NODE_HAS_STATUS_OKAY(DT_ALIAS(watchdog0))
#define APP_RECOVERY_WDT_NODE DT_ALIAS(watchdog0)
#elif DT_HAS_COMPAT_STATUS_OKAY(nordic_nrf_wdt)
#define APP_RECOVERY_WDT_NODE DT_COMPAT_GET_ANY_STATUS_OKAY(nordic_nrf_wdt)
#else
#define APP_RECOVERY_WDT_NODE DT_INVALID_NODE
#endif

static const struct app_recovery_config *recovery_manager_policy(
	const struct recovery_manager *manager)
{
	return &manager->app_context->config.recovery;
}

static uint32_t recovery_manager_evaluation_interval_ms(const struct recovery_manager *manager)
{
	const struct app_recovery_config *policy = recovery_manager_policy(manager);
	uint32_t interval_ms = MAX(policy->watchdog_timeout_ms / 4, 250);

	return MIN(interval_ms, (uint32_t)policy->watchdog_timeout_ms);
}

static bool recovery_manager_state_is_acceptable(enum network_connectivity_state state)
{
	return state == NETWORK_CONNECTIVITY_HEALTHY ||
	       state == NETWORK_CONNECTIVITY_LAN_UP_UPSTREAM_DEGRADED;
}

static bool recovery_manager_failure_equals(const struct network_failure_record *left,
					   const struct network_failure_record *right)
{
	return left->recorded == right->recorded &&
	       left->failure_stage == right->failure_stage &&
	       left->reason == right->reason;
}

static int64_t recovery_manager_elapsed_ms(int64_t started_at_ms)
{
	if (started_at_ms == 0) {
		return 0;
	}

	return k_uptime_get() - started_at_ms;
}

static void recovery_manager_schedule_evaluation(struct recovery_manager *manager,
						 k_timeout_t delay)
{
	k_work_reschedule(&manager->evaluation_work, delay);
}

static int recovery_manager_feed_watchdog(struct recovery_manager *manager)
{
	int ret;

	if (manager->watchdog == NULL || manager->watchdog_channel_id < 0) {
		return -ENODEV;
	}

	ret = wdt_feed(manager->watchdog, manager->watchdog_channel_id);
	if (ret != 0 && ret != -EAGAIN) {
		LOG_ERR("Recovery watchdog feed failed: %d", ret);
	}

	return ret;
}

static void recovery_manager_record_reset(struct recovery_manager *manager,
					 enum recovery_reset_trigger trigger)
{
	manager->last_reset_cause = (struct recovery_reset_cause){
		.available = true,
		.recovery_reset = true,
		.hardware_reset_cause = 0,
		.trigger = trigger,
		.failure_stage = manager->observed_failure.recorded
					 ? manager->observed_failure.failure_stage
					 : NETWORK_FAILURE_STAGE_NONE,
		.reason = manager->observed_failure.recorded ? manager->observed_failure.reason : 0,
		.connectivity_state = manager->observed_connectivity_state,
	};
}

static FUNC_NORETURN void recovery_manager_escalate(struct recovery_manager *manager,
					     enum recovery_reset_trigger trigger)
{
	recovery_manager_record_reset(manager, trigger);
	LOG_ERR("Recovery escalation trigger=%s state=%d stage=%d reason=%d",
		recovery_manager_reset_trigger_text(trigger),
		manager->last_reset_cause.connectivity_state,
		manager->last_reset_cause.failure_stage,
		manager->last_reset_cause.reason);
	sys_reboot(SYS_REBOOT_COLD);
}

static int recovery_manager_init_watchdog(struct recovery_manager *manager)
{
	const struct app_recovery_config *policy = recovery_manager_policy(manager);
	const struct device *watchdog = DEVICE_DT_GET_OR_NULL(APP_RECOVERY_WDT_NODE);
	struct wdt_timeout_cfg timeout_cfg = {
		.window = {
			.min = 0,
			.max = policy->watchdog_timeout_ms,
		},
		.callback = NULL,
		.flags = WDT_FLAG_RESET_SOC,
	};
	int ret;

	if (watchdog == NULL || !device_is_ready(watchdog)) {
		LOG_ERR("Recovery watchdog device not ready");
		return -ENODEV;
	}

	ret = wdt_install_timeout(watchdog, &timeout_cfg);
	if (ret < 0) {
		LOG_ERR("Recovery watchdog install failed: %d", ret);
		return ret;
	}

	manager->watchdog = watchdog;
	manager->watchdog_channel_id = ret;

	ret = wdt_setup(watchdog, WDT_OPT_PAUSE_HALTED_BY_DBG);
	if (ret == -ENOTSUP) {
		ret = wdt_setup(watchdog, 0);
	}

	if (ret != 0) {
		LOG_ERR("Recovery watchdog setup failed: %d", ret);
		return ret;
	}

	LOG_INF("Recovery watchdog armed timeout_ms=%d", policy->watchdog_timeout_ms);
	return 0;
}

static void recovery_manager_evaluate(struct recovery_manager *manager)
{
	const struct app_recovery_config *policy = recovery_manager_policy(manager);
	const struct network_runtime_state *network_state = &manager->app_context->network_state;
	bool acceptable_state = recovery_manager_state_is_acceptable(network_state->connectivity_state);
	bool startup_stuck;
	bool runtime_stuck;
	int ret;

	if (acceptable_state) {
		manager->degraded_since_ms = 0;
		if (manager->stable_since_ms == 0) {
			manager->stable_since_ms = k_uptime_get();
		}
	} else {
		manager->stable_since_ms = 0;
		if (manager->degraded_since_ms == 0) {
			manager->degraded_since_ms = k_uptime_get();
		}
	}

	startup_stuck = manager->startup_guard_active &&
		       recovery_manager_elapsed_ms(manager->last_progress_at_ms) >=
		       policy->degraded_patience_ms;
	runtime_stuck = manager->runtime_supervision_active && !acceptable_state &&
		       (recovery_manager_elapsed_ms(manager->last_progress_at_ms) >=
			policy->degraded_patience_ms ||
			recovery_manager_elapsed_ms(manager->degraded_since_ms) >=
			policy->degraded_patience_ms);

	if (startup_stuck || runtime_stuck) {
		recovery_manager_escalate(manager, RECOVERY_RESET_TRIGGER_CONFIRMED_STUCK);
	}

	ret = recovery_manager_feed_watchdog(manager);
	if (ret == 0 || ret == -EAGAIN) {
		recovery_manager_schedule_evaluation(
			manager,
			K_MSEC(recovery_manager_evaluation_interval_ms(manager)));
	}
}

static void recovery_manager_evaluation_work_handler(struct k_work *work)
{
	struct k_work_delayable *delayable = CONTAINER_OF(work, struct k_work_delayable, work);
	struct recovery_manager *manager =
		CONTAINER_OF(delayable, struct recovery_manager, evaluation_work);

	recovery_manager_evaluate(manager);
}

int recovery_manager_init(struct recovery_manager *manager, struct app_context *app_context)
{
	int ret;

	if (manager == NULL || app_context == NULL) {
		return -EINVAL;
	}

	*manager = (struct recovery_manager){
		.app_context = app_context,
		.watchdog_channel_id = -1,
		.observed_connectivity_state = NETWORK_CONNECTIVITY_NOT_READY,
		.last_progress_at_ms = k_uptime_get(),
	};
	k_work_init_delayable(&manager->evaluation_work, recovery_manager_evaluation_work_handler);

	ret = recovery_manager_init_watchdog(manager);
	if (ret != 0) {
		return ret;
	}

	return 0;
}

void recovery_manager_startup_begin(struct recovery_manager *manager)
{
	if (manager == NULL) {
		return;
	}

	manager->startup_guard_active = true;
	manager->runtime_supervision_active = false;
	manager->last_progress_at_ms = k_uptime_get();
	manager->degraded_since_ms = manager->last_progress_at_ms;
	manager->stable_since_ms = 0;
	recovery_manager_schedule_evaluation(manager, K_NO_WAIT);
}

void recovery_manager_startup_complete(struct recovery_manager *manager)
{
	if (manager == NULL) {
		return;
	}

	manager->startup_guard_active = false;
	manager->runtime_supervision_active = true;
	manager->last_progress_at_ms = k_uptime_get();
	manager->degraded_since_ms = 0;
	manager->stable_since_ms = 0;
	recovery_manager_schedule_evaluation(manager, K_NO_WAIT);
}

void recovery_manager_report_network_progress(struct recovery_manager *manager,
					      const struct network_runtime_state *network_state,
					      const char *reason)
{
	bool connectivity_changed;
	bool failure_changed;

	if (manager == NULL || network_state == NULL) {
		return;
	}

	connectivity_changed = manager->observed_connectivity_state != network_state->connectivity_state;
	failure_changed =
		!recovery_manager_failure_equals(&manager->observed_failure, &network_state->last_failure);

	manager->observed_connectivity_state = network_state->connectivity_state;
	manager->observed_failure = network_state->last_failure;

	if (reason != NULL || connectivity_changed || failure_changed) {
		manager->last_progress_at_ms = k_uptime_get();
		if (recovery_manager_state_is_acceptable(network_state->connectivity_state)) {
			manager->degraded_since_ms = 0;
			if (manager->stable_since_ms == 0) {
				manager->stable_since_ms = manager->last_progress_at_ms;
			}
		} else {
			manager->stable_since_ms = 0;
			if (manager->degraded_since_ms == 0) {
				manager->degraded_since_ms = manager->last_progress_at_ms;
			}
		}
	}

	recovery_manager_schedule_evaluation(manager, K_NO_WAIT);
}

const struct recovery_reset_cause *recovery_manager_last_reset_cause(
	const struct recovery_manager *manager)
{
	if (manager == NULL) {
		return NULL;
	}

	return &manager->last_reset_cause;
}

const char *recovery_manager_reset_trigger_text(enum recovery_reset_trigger trigger)
{
	switch (trigger) {
	case RECOVERY_RESET_TRIGGER_NONE:
		return "none";
	case RECOVERY_RESET_TRIGGER_CONFIRMED_STUCK:
		return "confirmed-stuck";
	case RECOVERY_RESET_TRIGGER_WATCHDOG_STARVATION:
		return "watchdog-starvation";
	default:
		return "unknown";
	}
}
