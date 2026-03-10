#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <zephyr/kernel.h>

#include "persistence/persistence_types.h"

struct app_context;

enum scheduler_clock_trust_state {
	SCHEDULER_CLOCK_TRUST_STATE_UNTRUSTED = 0,
	SCHEDULER_CLOCK_TRUST_STATE_TRUSTED,
	SCHEDULER_CLOCK_TRUST_STATE_DEGRADED,
};

enum scheduler_degraded_reason {
	SCHEDULER_DEGRADED_REASON_NONE = 0,
	SCHEDULER_DEGRADED_REASON_WAITING_FOR_TRUSTED_CLOCK,
	SCHEDULER_DEGRADED_REASON_INITIAL_TRUST_ACQUISITION_FAILED,
	SCHEDULER_DEGRADED_REASON_TRUSTED_CLOCK_UNAVAILABLE,
};

enum scheduler_last_result_code {
	SCHEDULER_LAST_RESULT_NONE = 0,
	SCHEDULER_LAST_RESULT_EXECUTED,
	SCHEDULER_LAST_RESULT_SKIPPED_UNTRUSTED_CLOCK,
	SCHEDULER_LAST_RESULT_SKIPPED_BASELINE_RESET,
	SCHEDULER_LAST_RESULT_FAILED,
};

enum scheduler_problem_code {
	SCHEDULER_PROBLEM_NONE = 0,
	SCHEDULER_PROBLEM_BACKWARD_UTC_CLOCK_JUMP,
	SCHEDULER_PROBLEM_NORMALIZED_UTC_MINUTE_CORRECTED,
	SCHEDULER_PROBLEM_FUTURE_ONLY_BASELINE_APPLIED,
};

struct scheduler_next_run {
	bool available;
	int64_t normalized_utc_minute;
	char schedule_id[PERSISTED_SCHEDULE_ID_MAX_LEN];
	char action_id[PERSISTED_ACTION_ID_MAX_LEN];
};

struct scheduler_last_result {
	bool available;
	enum scheduler_last_result_code code;
	int error_code;
	int64_t normalized_utc_minute;
	char schedule_id[PERSISTED_SCHEDULE_ID_MAX_LEN];
	char action_id[PERSISTED_ACTION_ID_MAX_LEN];
};

struct scheduler_problem_record {
	bool recorded;
	enum scheduler_problem_code code;
	int error_code;
	int64_t normalized_utc_minute;
	char schedule_id[PERSISTED_SCHEDULE_ID_MAX_LEN];
	char action_id[PERSISTED_ACTION_ID_MAX_LEN];
};

struct scheduler_runtime_status {
	bool implemented;
	bool utc_only;
	bool minute_resolution_only;
	bool automation_active;
	enum scheduler_clock_trust_state clock_state;
	enum scheduler_degraded_reason degraded_reason;
	uint32_t schedule_count;
	uint32_t enabled_schedule_count;
	struct scheduler_next_run next_run;
	struct scheduler_last_result last_result;
	uint32_t problem_count;
};

struct scheduler_service {
	struct app_context *app_context;
	struct k_mutex lock;
	struct scheduler_runtime_status status;
	struct scheduler_problem_record problems[10];
	uint32_t problem_head;
	bool baseline_valid;
	int64_t baseline_utc_minute;
};

const char *scheduler_clock_trust_state_text(
	enum scheduler_clock_trust_state state);

const char *scheduler_degraded_reason_text(enum scheduler_degraded_reason reason);

const char *scheduler_last_result_code_text(
	enum scheduler_last_result_code code);

int scheduler_service_init(struct scheduler_service *service,
			   struct app_context *app_context);

int scheduler_service_reload(struct scheduler_service *service);

int scheduler_service_mark_clock_untrusted(
	struct scheduler_service *service,
	enum scheduler_degraded_reason reason);

int scheduler_service_acquire_trusted_time(struct scheduler_service *service,
					   int64_t utc_epoch_seconds);

int scheduler_service_handle_clock_correction(
	struct scheduler_service *service,
	int64_t corrected_utc_epoch_seconds);

const struct scheduler_runtime_status *scheduler_service_get_status(
	const struct scheduler_service *service);
