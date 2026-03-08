#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/kernel.h>

#include "network/network_state.h"

struct app_context;

enum recovery_reset_trigger {
	RECOVERY_RESET_TRIGGER_NONE,
	RECOVERY_RESET_TRIGGER_CONFIRMED_STUCK,
	RECOVERY_RESET_TRIGGER_WATCHDOG_STARVATION,
};

struct recovery_reset_cause {
	bool available;
	bool recovery_reset;
	uint32_t hardware_reset_cause;
	enum recovery_reset_trigger trigger;
	enum network_failure_stage failure_stage;
	int reason;
	enum network_connectivity_state connectivity_state;
};

struct recovery_manager {
	struct app_context *app_context;
	const struct device *watchdog;
	struct k_work_delayable evaluation_work;
	struct recovery_reset_cause last_reset_cause;
	struct network_failure_record observed_failure;
	enum network_connectivity_state observed_connectivity_state;
	int watchdog_channel_id;
	int64_t last_progress_at_ms;
	int64_t degraded_since_ms;
	int64_t stable_since_ms;
	int64_t escalation_blocked_until_ms;
	bool startup_guard_active;
	bool runtime_supervision_active;
	bool prior_recovery_active;
};

int recovery_manager_init(struct recovery_manager *manager, struct app_context *app_context);
void recovery_manager_startup_begin(struct recovery_manager *manager);
void recovery_manager_startup_complete(struct recovery_manager *manager);
void recovery_manager_report_network_progress(struct recovery_manager *manager,
					      const struct network_runtime_state *network_state,
					      const char *reason);
const struct recovery_reset_cause *recovery_manager_last_reset_cause(
	const struct recovery_manager *manager);
const char *recovery_manager_reset_trigger_text(enum recovery_reset_trigger trigger);
