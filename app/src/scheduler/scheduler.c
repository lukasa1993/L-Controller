#include <errno.h>
#include <string.h>

#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include "app/app_context.h"
#include "scheduler/scheduler.h"

LOG_MODULE_REGISTER(scheduler, CONFIG_LOG_DEFAULT_LEVEL);

static int64_t scheduler_normalize_utc_minute(int64_t utc_epoch_seconds)
{
	if (utc_epoch_seconds >= 0) {
		return utc_epoch_seconds / 60;
	}

	return (utc_epoch_seconds - 59) / 60;
}

static bool scheduler_reason_is_degraded(enum scheduler_degraded_reason reason)
{
	switch (reason) {
	case SCHEDULER_DEGRADED_REASON_INITIAL_TRUST_ACQUISITION_FAILED:
	case SCHEDULER_DEGRADED_REASON_TRUSTED_CLOCK_UNAVAILABLE:
		return true;
	case SCHEDULER_DEGRADED_REASON_NONE:
	case SCHEDULER_DEGRADED_REASON_WAITING_FOR_TRUSTED_CLOCK:
	default:
		return false;
	}
}

static void scheduler_clear_next_run(struct scheduler_runtime_status *status)
{
	if (status == NULL) {
		return;
	}

	memset(&status->next_run, 0, sizeof(status->next_run));
	status->next_run.normalized_utc_minute = -1;
}

static void scheduler_clear_last_result(struct scheduler_runtime_status *status)
{
	if (status == NULL) {
		return;
	}

	memset(&status->last_result, 0, sizeof(status->last_result));
	status->last_result.code = SCHEDULER_LAST_RESULT_NONE;
	status->last_result.normalized_utc_minute = -1;
}

static void scheduler_record_problem_locked(
	struct scheduler_service *service,
	enum scheduler_problem_code code,
	int64_t normalized_utc_minute)
{
	struct scheduler_problem_record *problem;

	if (service == NULL || code == SCHEDULER_PROBLEM_NONE) {
		return;
	}

	problem = &service->problems[service->problem_head % ARRAY_SIZE(service->problems)];
	memset(problem, 0, sizeof(*problem));
	problem->recorded = true;
	problem->code = code;
	problem->normalized_utc_minute = normalized_utc_minute;

	service->problem_head = (service->problem_head + 1U) % ARRAY_SIZE(service->problems);
	if (service->status.problem_count < ARRAY_SIZE(service->problems)) {
		service->status.problem_count++;
	}
}

static void scheduler_refresh_counts_locked(struct scheduler_service *service)
{
	uint32_t count;
	uint32_t enabled_count = 0U;
	uint32_t max_count;
	uint32_t index;

	if (service == NULL || service->app_context == NULL) {
		return;
	}

	count = service->app_context->persisted_config.schedule.count;
	max_count = MIN(count, PERSISTED_SCHEDULE_MAX_COUNT);
	for (index = 0U; index < max_count; ++index) {
		if (service->app_context->persisted_config.schedule.entries[index].enabled) {
			enabled_count++;
		}
	}

	service->status.schedule_count = count;
	service->status.enabled_schedule_count = enabled_count;
}

static void scheduler_refresh_automation_locked(struct scheduler_service *service)
{
	if (service == NULL) {
		return;
	}

	service->status.automation_active =
		service->status.clock_state == SCHEDULER_CLOCK_TRUST_STATE_TRUSTED &&
		service->status.enabled_schedule_count > 0U;
}

static void scheduler_apply_future_only_baseline_locked(
	struct scheduler_service *service,
	int64_t normalized_utc_minute,
	enum scheduler_problem_code problem_code)
{
	if (service == NULL) {
		return;
	}

	service->baseline_valid = true;
	service->baseline_utc_minute = normalized_utc_minute;
	service->status.clock_state = SCHEDULER_CLOCK_TRUST_STATE_TRUSTED;
	service->status.degraded_reason = SCHEDULER_DEGRADED_REASON_NONE;
	service->status.last_result.available = true;
	service->status.last_result.code =
		SCHEDULER_LAST_RESULT_SKIPPED_BASELINE_RESET;
	service->status.last_result.error_code = 0;
	service->status.last_result.normalized_utc_minute = normalized_utc_minute;
	service->status.last_result.schedule_id[0] = '\0';
	service->status.last_result.action_id[0] = '\0';
	if (problem_code != SCHEDULER_PROBLEM_NONE) {
		scheduler_record_problem_locked(service, problem_code, normalized_utc_minute);
		scheduler_record_problem_locked(service,
					       SCHEDULER_PROBLEM_FUTURE_ONLY_BASELINE_APPLIED,
					       normalized_utc_minute);
	}

	scheduler_clear_next_run(&service->status);
	scheduler_refresh_automation_locked(service);
}

const char *scheduler_clock_trust_state_text(enum scheduler_clock_trust_state state)
{
	switch (state) {
	case SCHEDULER_CLOCK_TRUST_STATE_UNTRUSTED:
		return "untrusted";
	case SCHEDULER_CLOCK_TRUST_STATE_TRUSTED:
		return "trusted";
	case SCHEDULER_CLOCK_TRUST_STATE_DEGRADED:
		return "degraded";
	default:
		return "unknown";
	}
}

const char *scheduler_degraded_reason_text(enum scheduler_degraded_reason reason)
{
	switch (reason) {
	case SCHEDULER_DEGRADED_REASON_NONE:
		return "none";
	case SCHEDULER_DEGRADED_REASON_WAITING_FOR_TRUSTED_CLOCK:
		return "waiting-for-trusted-clock";
	case SCHEDULER_DEGRADED_REASON_INITIAL_TRUST_ACQUISITION_FAILED:
		return "initial-trust-acquisition-failed";
	case SCHEDULER_DEGRADED_REASON_TRUSTED_CLOCK_UNAVAILABLE:
		return "trusted-clock-unavailable";
	default:
		return "unknown";
	}
}

const char *scheduler_last_result_code_text(enum scheduler_last_result_code code)
{
	switch (code) {
	case SCHEDULER_LAST_RESULT_NONE:
		return "none";
	case SCHEDULER_LAST_RESULT_EXECUTED:
		return "executed";
	case SCHEDULER_LAST_RESULT_SKIPPED_UNTRUSTED_CLOCK:
		return "skipped-untrusted-clock";
	case SCHEDULER_LAST_RESULT_SKIPPED_BASELINE_RESET:
		return "skipped-baseline-reset";
	case SCHEDULER_LAST_RESULT_FAILED:
		return "failed";
	default:
		return "unknown";
	}
}

int scheduler_service_reload(struct scheduler_service *service)
{
	if (service == NULL || service->app_context == NULL) {
		return -EINVAL;
	}

	k_mutex_lock(&service->lock, K_FOREVER);
	scheduler_refresh_counts_locked(service);
	scheduler_refresh_automation_locked(service);
	if (service->status.clock_state != SCHEDULER_CLOCK_TRUST_STATE_TRUSTED) {
		scheduler_clear_next_run(&service->status);
	}
	k_mutex_unlock(&service->lock);

	return 0;
}

int scheduler_service_mark_clock_untrusted(
	struct scheduler_service *service,
	enum scheduler_degraded_reason reason)
{
	if (service == NULL) {
		return -EINVAL;
	}

	k_mutex_lock(&service->lock, K_FOREVER);
	service->baseline_valid = false;
	service->baseline_utc_minute = -1;
	service->status.clock_state = scheduler_reason_is_degraded(reason) ?
		SCHEDULER_CLOCK_TRUST_STATE_DEGRADED :
		SCHEDULER_CLOCK_TRUST_STATE_UNTRUSTED;
	service->status.degraded_reason =
		reason == SCHEDULER_DEGRADED_REASON_NONE ?
			SCHEDULER_DEGRADED_REASON_WAITING_FOR_TRUSTED_CLOCK : reason;
	scheduler_clear_next_run(&service->status);
	service->status.automation_active = false;
	k_mutex_unlock(&service->lock);

	return 0;
}

int scheduler_service_acquire_trusted_time(struct scheduler_service *service,
					   int64_t utc_epoch_seconds)
{
	if (service == NULL) {
		return -EINVAL;
	}

	k_mutex_lock(&service->lock, K_FOREVER);
	scheduler_apply_future_only_baseline_locked(
		service,
		scheduler_normalize_utc_minute(utc_epoch_seconds),
		SCHEDULER_PROBLEM_NONE);
	k_mutex_unlock(&service->lock);

	return 0;
}

int scheduler_service_handle_clock_correction(
	struct scheduler_service *service,
	int64_t corrected_utc_epoch_seconds)
{
	int64_t normalized_utc_minute;
	enum scheduler_problem_code problem_code;

	if (service == NULL) {
		return -EINVAL;
	}

	normalized_utc_minute = scheduler_normalize_utc_minute(corrected_utc_epoch_seconds);

	k_mutex_lock(&service->lock, K_FOREVER);
	problem_code = service->baseline_valid &&
		      normalized_utc_minute < service->baseline_utc_minute ?
		      SCHEDULER_PROBLEM_BACKWARD_UTC_CLOCK_JUMP :
		      SCHEDULER_PROBLEM_NORMALIZED_UTC_MINUTE_CORRECTED;
	scheduler_apply_future_only_baseline_locked(service, normalized_utc_minute,
					 problem_code);
	k_mutex_unlock(&service->lock);

	return 0;
}

const struct scheduler_runtime_status *scheduler_service_get_status(
	const struct scheduler_service *service)
{
	if (service == NULL) {
		return NULL;
	}

	return &service->status;
}

int scheduler_service_init(struct scheduler_service *service,
			   struct app_context *app_context)
{
	int ret;

	if (service == NULL || app_context == NULL) {
		return -EINVAL;
	}

	memset(service, 0, sizeof(*service));
	service->app_context = app_context;
	service->baseline_utc_minute = -1;
	k_mutex_init(&service->lock);

	service->status.implemented = true;
	service->status.utc_only = true;
	service->status.minute_resolution_only = true;
	service->status.clock_state = SCHEDULER_CLOCK_TRUST_STATE_UNTRUSTED;
	service->status.degraded_reason =
		SCHEDULER_DEGRADED_REASON_WAITING_FOR_TRUSTED_CLOCK;
	scheduler_clear_next_run(&service->status);
	scheduler_clear_last_result(&service->status);

	ret = scheduler_service_reload(service);
	if (ret != 0) {
		return ret;
	}

	LOG_INF("Scheduler service ready with trusted clock gate, UTC minute cadence, and future-only baseline semantics");
	return 0;
}
