#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <zephyr/kernel.h>

#include "persistence/persistence_types.h"

#define SCHEDULER_PROBLEM_HISTORY_CAPACITY 10U

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
	SCHEDULER_PROBLEM_TRUSTED_CLOCK_UNAVAILABLE,
	SCHEDULER_PROBLEM_ACTION_DISPATCH_FAILED,
};

struct scheduler_cron_matcher {
	uint64_t minute_bits;
	uint64_t hour_bits;
	uint64_t day_of_month_bits;
	uint64_t month_bits;
	uint64_t day_of_week_bits;
	bool day_of_month_wildcard;
	bool day_of_week_wildcard;
};

struct scheduler_runtime_entry {
	bool in_use;
	bool enabled;
	char schedule_id[PERSISTED_SCHEDULE_ID_MAX_LEN];
	char action_id[PERSISTED_ACTION_ID_MAX_LEN];
	char output_key[PERSISTED_ACTION_OUTPUT_KEY_MAX_LEN];
	char cron_expression[PERSISTED_SCHEDULE_CRON_EXPRESSION_MAX_LEN];
	enum persisted_action_command command;
	struct scheduler_cron_matcher cron;
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
	struct k_work_delayable runtime_work;
	struct scheduler_runtime_status status;
	struct scheduler_runtime_entry entries[PERSISTED_SCHEDULE_MAX_COUNT];
	struct scheduler_problem_record problems[SCHEDULER_PROBLEM_HISTORY_CAPACITY];
	uint32_t problem_head;
	uint32_t compiled_count;
	bool runtime_started;
	bool baseline_valid;
	int64_t baseline_utc_minute;
};

const char *scheduler_clock_trust_state_text(
	enum scheduler_clock_trust_state state);

const char *scheduler_degraded_reason_text(enum scheduler_degraded_reason reason);

const char *scheduler_last_result_code_text(
	enum scheduler_last_result_code code);

const char *scheduler_problem_code_text(enum scheduler_problem_code code);

int scheduler_cron_validate_expression(const char *expression);

int scheduler_schedule_table_validate(
	const struct persisted_schedule_table *schedule_table,
	const struct persisted_action_catalog *actions);

int scheduler_service_init(struct scheduler_service *service,
				   struct app_context *app_context);

int scheduler_service_start(struct scheduler_service *service);

int scheduler_service_reload(struct scheduler_service *service);

int scheduler_service_mark_clock_untrusted(
	struct scheduler_service *service,
	enum scheduler_degraded_reason reason);

int scheduler_service_acquire_trusted_time(struct scheduler_service *service,
					   int64_t utc_epoch_seconds);

int scheduler_service_handle_clock_correction(
	struct scheduler_service *service,
	int64_t corrected_utc_epoch_seconds);

int scheduler_service_copy_snapshot(
	const struct scheduler_service *service,
	struct scheduler_runtime_status *status_out,
	struct scheduler_problem_record *problems_out,
	size_t problem_capacity,
	uint32_t *problem_count_out);

const struct scheduler_runtime_status *scheduler_service_get_status(
	const struct scheduler_service *service);
