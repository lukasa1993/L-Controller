#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <zephyr/net/net_if.h>
#include <zephyr/net/sntp.h>
#include <zephyr/posix/time.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include "actions/actions.h"
#include "app/app_context.h"
#include "network/network_supervisor.h"
#include "relay/relay.h"
#include "scheduler/scheduler.h"

LOG_MODULE_REGISTER(scheduler, CONFIG_LOG_DEFAULT_LEVEL);

#define SCHEDULER_CONFLICT_SCAN_START_YEAR 2000
#define SCHEDULER_CONFLICT_SCAN_DAYS 146097U

struct scheduler_due_entry {
	char schedule_id[PERSISTED_SCHEDULE_ID_MAX_LEN];
	char action_id[PERSISTED_ACTION_ID_MAX_LEN];
};

static void scheduler_record_problem_locked(
	struct scheduler_service *service,
	enum scheduler_problem_code code,
	int64_t normalized_utc_minute,
	int error_code,
	const char *schedule_id,
	const char *action_id);

static int scheduler_runtime_entry_next_utc_minute(
	const struct scheduler_runtime_entry *entry,
	int64_t after_utc_minute,
	int64_t *next_utc_minute);

static void scheduler_set_last_result_locked(
	struct scheduler_service *service,
	enum scheduler_last_result_code code,
	int error_code,
	int64_t normalized_utc_minute,
	const char *schedule_id,
	const char *action_id);

static void scheduler_refresh_automation_locked(struct scheduler_service *service);
static void scheduler_refresh_next_run_locked(struct scheduler_service *service);

static int64_t scheduler_normalize_utc_minute(int64_t utc_epoch_seconds)
{
	if (utc_epoch_seconds >= 0) {
		return utc_epoch_seconds / 60;
	}

	return (utc_epoch_seconds - 59) / 60;
}

static bool scheduler_c_string_has_terminator(const char *value, size_t value_len)
{
	return value != NULL && memchr(value, '\0', value_len) != NULL;
}

static bool scheduler_c_string_is_non_empty(const char *value, size_t value_len)
{
	return value != NULL && value[0] != '\0' &&
	       scheduler_c_string_has_terminator(value, value_len);
}

static int scheduler_copy_c_string(char *dst, size_t dst_len, const char *src)
{
	size_t src_len;

	if (dst == NULL || src == NULL || dst_len == 0U) {
		return -EINVAL;
	}

	src_len = strlen(src);
	if (src_len >= dst_len) {
		return -ENAMETOOLONG;
	}

	memcpy(dst, src, src_len + 1U);
	return 0;
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

static bool scheduler_service_prior_clock_reset(const struct scheduler_service *service)
{
	const struct recovery_reset_cause *reset_cause;

	if (service == NULL || service->app_context == NULL) {
		return false;
	}

	reset_cause = recovery_manager_last_reset_cause(&service->app_context->recovery);
	return reset_cause != NULL && reset_cause->available && reset_cause->recovery_reset &&
	       reset_cause->trigger == RECOVERY_RESET_TRIGGER_TRUSTED_CLOCK_ACQUISITION;
}

static int scheduler_service_query_trusted_clock(
	const struct scheduler_service *service,
	struct sntp_time *timestamp)
{
	const struct net_if *wifi_iface;
	const char *fallback_server;
	uint32_t timeout_ms;

	if (service == NULL || service->app_context == NULL || timestamp == NULL) {
		return -EINVAL;
	}

	wifi_iface = service->app_context->network_state.wifi_iface;
	timeout_ms =
		(uint32_t)MAX(service->app_context->config.scheduler.trusted_clock_timeout_ms, 1000);

	if (wifi_iface != NULL &&
	    !net_ipv4_is_addr_unspecified(&wifi_iface->config.dhcpv4.ntp_addr)) {
		struct sockaddr_in sntp_addr = { 0 };

		sntp_addr.sin_family = AF_INET;
		sntp_addr.sin_addr.s_addr = wifi_iface->config.dhcpv4.ntp_addr.s_addr;
		return sntp_simple_addr((struct sockaddr *)&sntp_addr, sizeof(sntp_addr), timeout_ms,
					 timestamp);
	}

	fallback_server = service->app_context->config.scheduler.trusted_clock_server;
	if (fallback_server == NULL || fallback_server[0] == '\0') {
		return -EINVAL;
	}

	return sntp_simple(fallback_server, timeout_ms, timestamp);
}

static int scheduler_service_sync_clock(
	struct scheduler_service *service,
	int64_t *utc_epoch_seconds)
{
	struct network_supervisor_status network_status;
	struct sntp_time timestamp;
	struct timespec realtime = { 0 };
	int ret;

	if (service == NULL || service->app_context == NULL) {
		return -EINVAL;
	}

	ret = network_supervisor_get_status(&service->app_context->network_state,
					   &network_status);
	if (ret != 0) {
		return ret;
	}

	if (network_status.connectivity_state != NETWORK_CONNECTIVITY_HEALTHY) {
		return -ENETUNREACH;
	}

	ret = scheduler_service_query_trusted_clock(service, &timestamp);
	if (ret != 0) {
		return ret;
	}

	realtime.tv_sec = (time_t)timestamp.seconds;
	realtime.tv_nsec = ((uint64_t)timestamp.fraction * 1000000000ULL) >> 32;
	ret = clock_settime(CLOCK_REALTIME, &realtime);
	if (ret != 0) {
		return errno != 0 ? -errno : -EIO;
	}

	if (utc_epoch_seconds != NULL) {
		*utc_epoch_seconds = (int64_t)realtime.tv_sec;
	}

	return 0;
}

static int scheduler_service_read_realtime(
	int64_t *utc_epoch_seconds,
	int64_t *normalized_utc_minute)
{
	struct timespec realtime = { 0 };
	int ret;

	if (utc_epoch_seconds == NULL || normalized_utc_minute == NULL) {
		return -EINVAL;
	}

	ret = clock_gettime(CLOCK_REALTIME, &realtime);
	if (ret != 0) {
		return errno != 0 ? -errno : -EIO;
	}

	*utc_epoch_seconds = (int64_t)realtime.tv_sec;
	*normalized_utc_minute = scheduler_normalize_utc_minute(*utc_epoch_seconds);
	return 0;
}

static k_timeout_t scheduler_service_runtime_delay(const struct scheduler_service *service)
{
	const int64_t cadence_ms = (int64_t)MAX(service->app_context->config.scheduler.cadence_seconds, 1U) *
		1000LL;
	int64_t utc_epoch_seconds;
	int64_t normalized_utc_minute;
	int ret;

	if (service == NULL || service->app_context == NULL) {
		return K_NO_WAIT;
	}

	if (service->status.clock_state != SCHEDULER_CLOCK_TRUST_STATE_TRUSTED) {
		return K_MSEC(cadence_ms);
	}

	ret = scheduler_service_read_realtime(&utc_epoch_seconds, &normalized_utc_minute);
	if (ret != 0) {
		return K_MSEC(cadence_ms);
	}

	return K_MSEC((((normalized_utc_minute + 1LL) * 60LL) - utc_epoch_seconds) * 1000LL);
}

static void scheduler_service_schedule_runtime_work(struct scheduler_service *service)
{
	k_timeout_t delay;

	if (service == NULL || !service->runtime_started) {
		return;
	}

	delay = scheduler_service_runtime_delay(service);
	if (K_TIMEOUT_EQ(delay, K_NO_WAIT)) {
		delay = K_SECONDS(service->app_context->config.scheduler.cadence_seconds);
	}

	k_work_reschedule(&service->runtime_work, delay);
}

static void scheduler_service_advance_baseline_locked(
	struct scheduler_service *service,
	int64_t normalized_utc_minute)
{
	if (service == NULL) {
		return;
	}

	service->baseline_valid = true;
	service->baseline_utc_minute = normalized_utc_minute;
	scheduler_refresh_automation_locked(service);
	scheduler_refresh_next_run_locked(service);
}

static uint32_t scheduler_collect_due_entries_locked(
	struct scheduler_service *service,
	int64_t normalized_utc_minute,
	struct scheduler_due_entry due_entries[PERSISTED_SCHEDULE_MAX_COUNT])
{
	uint32_t due_count = 0U;
	uint32_t index;

	if (service == NULL || due_entries == NULL || !service->baseline_valid) {
		return 0U;
	}

	for (index = 0U; index < service->compiled_count; ++index) {
		int64_t due_utc_minute;
		int ret;

		if (!service->entries[index].in_use || !service->entries[index].enabled) {
			continue;
		}

		ret = scheduler_runtime_entry_next_utc_minute(
			&service->entries[index],
			service->baseline_utc_minute,
			&due_utc_minute);
		if (ret != 0 || due_utc_minute != normalized_utc_minute) {
			continue;
		}

		memcpy(due_entries[due_count].schedule_id,
		       service->entries[index].schedule_id,
		       sizeof(due_entries[due_count].schedule_id));
		memcpy(due_entries[due_count].action_id,
		       service->entries[index].action_id,
		       sizeof(due_entries[due_count].action_id));
		due_count++;
	}

	return due_count;
}

static void scheduler_execute_due_minute(
	struct scheduler_service *service,
	int64_t normalized_utc_minute)
{
	struct scheduler_due_entry due_entries[PERSISTED_SCHEDULE_MAX_COUNT];
	uint32_t due_count;
	uint32_t index;

	if (service == NULL || service->app_context == NULL) {
		return;
	}

	memset(due_entries, 0, sizeof(due_entries));

	k_mutex_lock(&service->lock, K_FOREVER);
	due_count = scheduler_collect_due_entries_locked(service, normalized_utc_minute, due_entries);
	k_mutex_unlock(&service->lock);

	for (index = 0U; index < due_count; ++index) {
		struct action_dispatch_result dispatch_result;
		int ret;

		ret = action_dispatcher_execute(&service->app_context->actions,
					       due_entries[index].action_id,
					       ACTION_DISPATCH_SOURCE_SCHEDULER,
					       &dispatch_result);

		k_mutex_lock(&service->lock, K_FOREVER);
		if (ret == 0 && dispatch_result.accepted) {
			scheduler_set_last_result_locked(
				service,
				SCHEDULER_LAST_RESULT_EXECUTED,
				0,
				normalized_utc_minute,
				due_entries[index].schedule_id,
				due_entries[index].action_id);
		} else {
			const int error_code = ret != 0 ? ret :
				(dispatch_result.error_code != 0 ? dispatch_result.error_code : -EIO);

			scheduler_set_last_result_locked(
				service,
				SCHEDULER_LAST_RESULT_FAILED,
				error_code,
				normalized_utc_minute,
				due_entries[index].schedule_id,
				due_entries[index].action_id);
			scheduler_record_problem_locked(
				service,
				SCHEDULER_PROBLEM_ACTION_DISPATCH_FAILED,
				normalized_utc_minute,
				error_code,
				due_entries[index].schedule_id,
				due_entries[index].action_id);
		}
		k_mutex_unlock(&service->lock);
	}

	k_mutex_lock(&service->lock, K_FOREVER);
	scheduler_service_advance_baseline_locked(service, normalized_utc_minute);
	k_mutex_unlock(&service->lock);
}

static void scheduler_service_runtime_work_handler(struct k_work *work)
{
	struct k_work_delayable *delayable = CONTAINER_OF(work, struct k_work_delayable, work);
	struct scheduler_service *service =
		CONTAINER_OF(delayable, struct scheduler_service, runtime_work);
	int64_t utc_epoch_seconds = -1;
	int64_t normalized_utc_minute = -1;
	int64_t baseline_utc_minute = -1;
	enum scheduler_clock_trust_state clock_state;
	bool baseline_valid;
	int ret;

	if (service == NULL || !service->runtime_started) {
		return;
	}

	k_mutex_lock(&service->lock, K_FOREVER);
	clock_state = service->status.clock_state;
	baseline_valid = service->baseline_valid;
	baseline_utc_minute = service->baseline_utc_minute;
	k_mutex_unlock(&service->lock);

	if (clock_state != SCHEDULER_CLOCK_TRUST_STATE_TRUSTED) {
		ret = scheduler_service_sync_clock(service, &utc_epoch_seconds);
		if (ret == 0) {
			(void)scheduler_service_acquire_trusted_time(service, utc_epoch_seconds);
			} else {
				k_mutex_lock(&service->lock, K_FOREVER);
				scheduler_record_problem_locked(service,
					       SCHEDULER_PROBLEM_TRUSTED_CLOCK_UNAVAILABLE,
					       -1,
					       ret,
					       NULL,
					       NULL);
				k_mutex_unlock(&service->lock);
			}

		scheduler_service_schedule_runtime_work(service);
		return;
	}

	ret = scheduler_service_read_realtime(&utc_epoch_seconds, &normalized_utc_minute);
	if (ret != 0) {
		(void)scheduler_service_mark_clock_untrusted(
			service,
			SCHEDULER_DEGRADED_REASON_TRUSTED_CLOCK_UNAVAILABLE);
		k_mutex_lock(&service->lock, K_FOREVER);
		scheduler_record_problem_locked(service,
				       SCHEDULER_PROBLEM_TRUSTED_CLOCK_UNAVAILABLE,
				       -1,
				       ret,
				       NULL,
				       NULL);
		k_mutex_unlock(&service->lock);
		scheduler_service_schedule_runtime_work(service);
		return;
	}

	if (!baseline_valid) {
		(void)scheduler_service_acquire_trusted_time(service, utc_epoch_seconds);
		scheduler_service_schedule_runtime_work(service);
		return;
	}

	if (normalized_utc_minute < baseline_utc_minute ||
	    normalized_utc_minute > baseline_utc_minute + 1LL) {
		(void)scheduler_service_handle_clock_correction(service, utc_epoch_seconds);
		scheduler_service_schedule_runtime_work(service);
		return;
	}

	if (normalized_utc_minute == baseline_utc_minute + 1LL) {
		scheduler_execute_due_minute(service, normalized_utc_minute);
	}

	scheduler_service_schedule_runtime_work(service);
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

static void scheduler_set_last_result_locked(
	struct scheduler_service *service,
	enum scheduler_last_result_code code,
	int error_code,
	int64_t normalized_utc_minute,
	const char *schedule_id,
	const char *action_id)
{
	if (service == NULL) {
		return;
	}

	service->status.last_result.available = true;
	service->status.last_result.code = code;
	service->status.last_result.error_code = error_code;
	service->status.last_result.normalized_utc_minute = normalized_utc_minute;
	service->status.last_result.schedule_id[0] = '\0';
	service->status.last_result.action_id[0] = '\0';

	if (schedule_id != NULL) {
		strncpy(service->status.last_result.schedule_id,
			schedule_id,
			sizeof(service->status.last_result.schedule_id) - 1U);
	}

	if (action_id != NULL) {
		strncpy(service->status.last_result.action_id,
			action_id,
			sizeof(service->status.last_result.action_id) - 1U);
	}
}

static void scheduler_record_problem_locked(
	struct scheduler_service *service,
	enum scheduler_problem_code code,
	int64_t normalized_utc_minute,
	int error_code,
	const char *schedule_id,
	const char *action_id)
{
	struct scheduler_problem_record *problem;

	if (service == NULL || code == SCHEDULER_PROBLEM_NONE) {
		return;
	}

	problem =
		&service->problems[service->problem_head % ARRAY_SIZE(service->problems)];
	memset(problem, 0, sizeof(*problem));
	problem->recorded = true;
	problem->code = code;
	problem->error_code = error_code;
	problem->normalized_utc_minute = normalized_utc_minute;

	if (schedule_id != NULL) {
		strncpy(problem->schedule_id, schedule_id, sizeof(problem->schedule_id) - 1U);
	}

	if (action_id != NULL) {
		strncpy(problem->action_id, action_id, sizeof(problem->action_id) - 1U);
	}

	service->problem_head =
		(service->problem_head + 1U) % ARRAY_SIZE(service->problems);
	if (service->status.problem_count < ARRAY_SIZE(service->problems)) {
		service->status.problem_count++;
	}
}

static void scheduler_refresh_counts_locked(struct scheduler_service *service)
{
	uint32_t enabled_count = 0U;
	uint32_t index;

	if (service == NULL) {
		return;
	}

	for (index = 0U; index < service->compiled_count; ++index) {
		if (service->entries[index].in_use && service->entries[index].enabled) {
			enabled_count++;
		}
	}

	service->status.schedule_count = service->compiled_count;
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

static bool scheduler_value_is_allowed(uint64_t bits, uint8_t value)
{
	return (bits & BIT64(value)) != 0U;
}

static int scheduler_parse_u8_token(
	const char *text,
	uint8_t min_value,
	uint8_t max_value,
	uint8_t *value)
{
	char *endptr = NULL;
	unsigned long parsed;

	if (text == NULL || value == NULL || text[0] == '\0') {
		return -EINVAL;
	}

	errno = 0;
	parsed = strtoul(text, &endptr, 10);
	if (errno != 0 || endptr == text || *endptr != '\0' ||
	    parsed < min_value || parsed > max_value) {
		return -EINVAL;
	}

	*value = (uint8_t)parsed;
	return 0;
}

static int scheduler_compile_field(
	const char *field_text,
	uint8_t min_value,
	uint8_t max_value,
	bool allow_sunday_alias,
	uint64_t *bits,
	bool *wildcard)
{
	char field_copy[PERSISTED_SCHEDULE_CRON_EXPRESSION_MAX_LEN];
	char *token;
	char *saveptr = NULL;
	bool saw_star_token = false;
	bool saw_token = false;
	int ret;

	if (field_text == NULL || bits == NULL) {
		return -EINVAL;
	}

	ret = scheduler_copy_c_string(field_copy, sizeof(field_copy), field_text);
	if (ret != 0) {
		return ret;
	}

	*bits = 0U;
	if (wildcard != NULL) {
		*wildcard = strcmp(field_text, "*") == 0;
	}

	for (token = strtok_r(field_copy, ",", &saveptr); token != NULL;
	     token = strtok_r(NULL, ",", &saveptr)) {
		char *slash = strchr(token, '/');
		char *dash = NULL;
		uint8_t start_value;
		uint8_t end_value;
		uint8_t step_value = 1U;
		const char *step_text = NULL;
		uint16_t current;

		if (slash != NULL) {
			*slash = '\0';
			step_text = slash + 1;
			ret = scheduler_parse_u8_token(step_text, 1U, UINT8_MAX,
					      &step_value);
			if (ret != 0) {
				return ret;
			}
		}

		if (token[0] == '\0') {
			return -EINVAL;
		}

		if (strcmp(token, "*") == 0) {
			if (saw_token) {
				return -EINVAL;
			}

			saw_star_token = true;
			start_value = min_value;
			end_value = max_value;
		} else {
			if (saw_star_token) {
				return -EINVAL;
			}

			dash = strchr(token, '-');
			if (dash != NULL) {
				*dash = '\0';
				if (strchr(dash + 1, '-') != NULL) {
					return -EINVAL;
				}

				ret = scheduler_parse_u8_token(token, min_value, max_value,
						      &start_value);
				if (ret != 0) {
					return ret;
				}

				ret = scheduler_parse_u8_token(dash + 1, min_value, max_value,
						      &end_value);
				if (ret != 0 || start_value > end_value) {
					return -EINVAL;
				}
			} else {
				if (step_text != NULL) {
					return -EINVAL;
				}

				ret = scheduler_parse_u8_token(token, min_value, max_value,
						      &start_value);
				if (ret != 0) {
					return ret;
				}

				end_value = start_value;
			}
		}

		for (current = start_value; current <= end_value; current += step_value) {
			uint8_t normalized_value = (uint8_t)current;

			if (allow_sunday_alias && normalized_value == 7U) {
				normalized_value = 0U;
			}

			*bits |= BIT64(normalized_value);
		}

		saw_token = true;
	}

	return saw_token ? 0 : -EINVAL;
}

static int scheduler_compile_expression(
	const char *expression,
	struct scheduler_cron_matcher *cron)
{
	char expression_copy[PERSISTED_SCHEDULE_CRON_EXPRESSION_MAX_LEN];
	char *fields[5] = { 0 };
	char *token;
	char *saveptr = NULL;
	bool unused_wildcard;
	int field_count = 0;
	int ret;

	if (!scheduler_c_string_is_non_empty(
		    expression, PERSISTED_SCHEDULE_CRON_EXPRESSION_MAX_LEN) ||
	    cron == NULL) {
		return -EINVAL;
	}

	ret = scheduler_copy_c_string(expression_copy, sizeof(expression_copy),
				     expression);
	if (ret != 0) {
		return ret;
	}

	for (token = strtok_r(expression_copy, " \t\r\n", &saveptr);
	     token != NULL; token = strtok_r(NULL, " \t\r\n", &saveptr)) {
		if (field_count >= ARRAY_SIZE(fields)) {
			return -EINVAL;
		}

		fields[field_count++] = token;
	}

	if (field_count != ARRAY_SIZE(fields)) {
		return -EINVAL;
	}

	memset(cron, 0, sizeof(*cron));

	ret = scheduler_compile_field(fields[0], 0U, 59U, false,
				     &cron->minute_bits, &unused_wildcard);
	if (ret != 0) {
		return ret;
	}

	ret = scheduler_compile_field(fields[1], 0U, 23U, false,
				     &cron->hour_bits, &unused_wildcard);
	if (ret != 0) {
		return ret;
	}

	ret = scheduler_compile_field(fields[2], 1U, 31U, false,
				     &cron->day_of_month_bits,
				     &cron->day_of_month_wildcard);
	if (ret != 0) {
		return ret;
	}

	ret = scheduler_compile_field(fields[3], 1U, 12U, false,
				     &cron->month_bits, &unused_wildcard);
	if (ret != 0) {
		return ret;
	}

	return scheduler_compile_field(fields[4], 0U, 7U, true,
				       &cron->day_of_week_bits,
				       &cron->day_of_week_wildcard);
}

static bool scheduler_cron_date_matches(
	const struct scheduler_cron_matcher *cron,
	uint8_t day_of_month,
	uint8_t day_of_week)
{
	const bool dom_match =
		scheduler_value_is_allowed(cron->day_of_month_bits, day_of_month);
	const bool dow_match =
		scheduler_value_is_allowed(cron->day_of_week_bits, day_of_week);

	if (cron->day_of_month_wildcard && cron->day_of_week_wildcard) {
		return true;
	}

	if (cron->day_of_month_wildcard) {
		return dow_match;
	}

	if (cron->day_of_week_wildcard) {
		return dom_match;
	}

	return dom_match || dow_match;
}

static bool scheduler_is_leap_year(int year)
{
	if ((year % 400) == 0) {
		return true;
	}

	if ((year % 100) == 0) {
		return false;
	}

	return (year % 4) == 0;
}

static uint8_t scheduler_days_in_month(int year, uint8_t month)
{
	static const uint8_t month_lengths[] = {
		31U, 28U, 31U, 30U, 31U, 30U,
		31U, 31U, 30U, 31U, 30U, 31U,
	};

	if (month == 2U && scheduler_is_leap_year(year)) {
		return 29U;
	}

	return month_lengths[month - 1U];
}

static void scheduler_increment_utc_date(
	int *year,
	uint8_t *month,
	uint8_t *day,
	uint8_t *day_of_week)
{
	const uint8_t month_length = scheduler_days_in_month(*year, *month);

	*day += 1U;
	*day_of_week = (uint8_t)((*day_of_week + 1U) % 7U);
	if (*day <= month_length) {
		return;
	}

	*day = 1U;
	*month += 1U;
	if (*month <= 12U) {
		return;
	}

	*month = 1U;
	*year += 1;
}

static bool scheduler_runtime_entries_conflict(
	const struct scheduler_runtime_entry *first,
	const struct scheduler_runtime_entry *second)
{
	int year = SCHEDULER_CONFLICT_SCAN_START_YEAR;
	uint8_t month = 1U;
	uint8_t day = 1U;
	uint8_t day_of_week = 6U;
	uint32_t scan_day;

	if (first == NULL || second == NULL || !first->enabled || !second->enabled ||
	    strcmp(first->output_key, second->output_key) != 0 ||
	    first->command == second->command) {
		return false;
	}

	if ((first->cron.minute_bits & second->cron.minute_bits) == 0U ||
	    (first->cron.hour_bits & second->cron.hour_bits) == 0U ||
	    (first->cron.month_bits & second->cron.month_bits) == 0U) {
		return false;
	}

	for (scan_day = 0U; scan_day < SCHEDULER_CONFLICT_SCAN_DAYS; ++scan_day) {
		if (scheduler_value_is_allowed(first->cron.month_bits, month) &&
		    scheduler_value_is_allowed(second->cron.month_bits, month) &&
		    scheduler_cron_date_matches(&first->cron, day, day_of_week) &&
		    scheduler_cron_date_matches(&second->cron, day, day_of_week)) {
			LOG_WRN("Rejecting cron conflict for %s/%s and %s/%s",
				first->schedule_id,
				first->action_id,
				second->schedule_id,
				second->action_id);
			return true;
		}

		scheduler_increment_utc_date(&year, &month, &day, &day_of_week);
	}

	return false;
}

static int scheduler_compile_runtime_entry(
	const struct persisted_schedule *schedule,
	const struct persisted_action_catalog *actions,
	struct scheduler_runtime_entry *entry)
{
	struct persisted_action action;
	int ret;

	if (schedule == NULL || actions == NULL || entry == NULL) {
		return -EINVAL;
	}

	if (!scheduler_c_string_is_non_empty(schedule->schedule_id,
					   sizeof(schedule->schedule_id)) ||
	    !scheduler_c_string_is_non_empty(schedule->action_id,
					   sizeof(schedule->action_id)) ||
	    !scheduler_c_string_is_non_empty(schedule->cron_expression,
					   sizeof(schedule->cron_expression))) {
		return -EINVAL;
	}

	ret = action_dispatcher_resolve_action(actions, schedule->action_id, true, &action);
	if (ret != 0) {
		return ret;
	}

	if (!action_dispatcher_action_record_valid(&action, true)) {
		return -EINVAL;
	}

	memset(entry, 0, sizeof(*entry));
	entry->in_use = true;
	entry->enabled = schedule->enabled;
	memcpy(entry->schedule_id, schedule->schedule_id, sizeof(entry->schedule_id));
	memcpy(entry->action_id, schedule->action_id, sizeof(entry->action_id));
	memcpy(entry->output_key, action.output_key, sizeof(entry->output_key));
	memcpy(entry->cron_expression, schedule->cron_expression,
	       sizeof(entry->cron_expression));
	entry->command = action.command;

	ret = scheduler_compile_expression(schedule->cron_expression, &entry->cron);
	if (ret != 0) {
		return ret;
	}

	return 0;
}

static int scheduler_compile_schedule_table(
	const struct persisted_schedule_table *schedule_table,
	const struct persisted_action_catalog *actions,
	struct scheduler_runtime_entry entries[PERSISTED_SCHEDULE_MAX_COUNT],
	uint32_t *count_out,
	uint32_t *enabled_count_out)
{
	uint32_t enabled_count = 0U;
	uint32_t index;
	uint32_t match_index;
	int ret;

	if (schedule_table == NULL || actions == NULL || entries == NULL) {
		return -EINVAL;
	}

	if (schedule_table->count > PERSISTED_SCHEDULE_MAX_COUNT) {
		return -EINVAL;
	}

	memset(entries, 0,
	       sizeof(struct scheduler_runtime_entry) * PERSISTED_SCHEDULE_MAX_COUNT);

	for (index = 0U; index < schedule_table->count; ++index) {
		const struct persisted_schedule *schedule = &schedule_table->entries[index];

		ret = scheduler_compile_runtime_entry(schedule, actions, &entries[index]);
		if (ret != 0) {
			return ret;
		}

		for (match_index = 0U; match_index < index; ++match_index) {
			if (strcmp(entries[index].schedule_id,
				   entries[match_index].schedule_id) == 0) {
				return -EEXIST;
			}
		}

		if (entries[index].enabled) {
			enabled_count++;
		}
	}

	for (index = 0U; index < schedule_table->count; ++index) {
		for (match_index = index + 1U; match_index < schedule_table->count;
		     ++match_index) {
			if (scheduler_runtime_entries_conflict(&entries[index],
						      &entries[match_index])) {
				return -EADDRINUSE;
			}
		}
	}

	if (count_out != NULL) {
		*count_out = schedule_table->count;
	}

	if (enabled_count_out != NULL) {
		*enabled_count_out = enabled_count;
	}

	return 0;
}

static int64_t scheduler_days_from_civil(int year, unsigned month, unsigned day)
{
	const unsigned adjusted_month = month > 2U ? month - 3U : month + 9U;
	const int adjusted_year = year - (month <= 2U ? 1 : 0);
	const int era =
		(adjusted_year >= 0 ? adjusted_year : adjusted_year - 399) / 400;
	const unsigned year_of_era =
		(unsigned)(adjusted_year - (era * 400));
	const unsigned day_of_year =
		((153U * adjusted_month) + 2U) / 5U + day - 1U;
	const unsigned day_of_era =
		year_of_era * 365U + year_of_era / 4U - year_of_era / 100U +
		day_of_year;

	return ((int64_t)era * 146097) + (int64_t)day_of_era - 719468;
}

static int64_t scheduler_tm_to_epoch_minute(const struct tm *utc_tm)
{
	return (scheduler_days_from_civil(utc_tm->tm_year + 1900,
					  (unsigned)utc_tm->tm_mon + 1U,
					  (unsigned)utc_tm->tm_mday) * 1440) +
	       ((int64_t)utc_tm->tm_hour * 60) + utc_tm->tm_min;
}

static int scheduler_find_next_value(
	uint64_t bits,
	uint8_t start_value,
	uint8_t max_value,
	uint8_t *value)
{
	uint16_t candidate;

	if (value == NULL) {
		return -EINVAL;
	}

	for (candidate = start_value; candidate <= max_value; ++candidate) {
		if (scheduler_value_is_allowed(bits, (uint8_t)candidate)) {
			*value = (uint8_t)candidate;
			return 0;
		}
	}

	return -ENOENT;
}

static int scheduler_runtime_entry_next_utc_minute(
	const struct scheduler_runtime_entry *entry,
	int64_t after_utc_minute,
	int64_t *next_utc_minute)
{
	struct tm current_tm;
	time_t current_seconds;
	int year;
	uint8_t month;
	uint8_t day;
	uint8_t day_of_week;
	uint8_t start_hour;
	uint8_t start_minute;
	uint32_t scan_day;

	if (entry == NULL || next_utc_minute == NULL || !entry->enabled ||
	    after_utc_minute < 0) {
		return -EINVAL;
	}

	current_seconds = (time_t)((after_utc_minute + 1LL) * 60LL);
	if (gmtime_r(&current_seconds, &current_tm) == NULL) {
		return -ERANGE;
	}

	year = current_tm.tm_year + 1900;
	month = (uint8_t)current_tm.tm_mon + 1U;
	day = (uint8_t)current_tm.tm_mday;
	day_of_week = (uint8_t)current_tm.tm_wday;
	start_hour = (uint8_t)current_tm.tm_hour;
	start_minute = (uint8_t)current_tm.tm_min;

	for (scan_day = 0U; scan_day < SCHEDULER_CONFLICT_SCAN_DAYS; ++scan_day) {
		uint8_t candidate_hour = 0U;
		int hour_ret;

		if (scheduler_value_is_allowed(entry->cron.month_bits, month) &&
		    scheduler_cron_date_matches(&entry->cron, day, day_of_week)) {
			hour_ret = scheduler_find_next_value(entry->cron.hour_bits, start_hour,
						     23U, &candidate_hour);
			while (hour_ret == 0) {
				uint8_t candidate_minute;
				struct tm candidate_tm = {
					.tm_year = year - 1900,
					.tm_mon = month - 1U,
					.tm_mday = day,
					.tm_hour = candidate_hour,
					.tm_min = 0,
					.tm_sec = 0,
				};
				int minute_ret = scheduler_find_next_value(
					entry->cron.minute_bits,
					candidate_hour == start_hour ? start_minute : 0U,
					59U,
					&candidate_minute);

				if (minute_ret == 0) {
					candidate_tm.tm_min = candidate_minute;
					*next_utc_minute =
						scheduler_tm_to_epoch_minute(&candidate_tm);
					if (*next_utc_minute > after_utc_minute) {
						return 0;
					}
				}

				hour_ret = scheduler_find_next_value(entry->cron.hour_bits,
							candidate_hour + 1U,
							23U,
							&candidate_hour);
			}
		}

		scheduler_increment_utc_date(&year, &month, &day, &day_of_week);
		start_hour = 0U;
		start_minute = 0U;
	}

	return -ENOENT;
}

static void scheduler_refresh_next_run_locked(struct scheduler_service *service)
{
	bool found = false;
	int64_t earliest_utc_minute = -1;
	uint32_t index;

	if (service == NULL) {
		return;
	}

	scheduler_clear_next_run(&service->status);
	if (!service->baseline_valid ||
	    service->status.clock_state != SCHEDULER_CLOCK_TRUST_STATE_TRUSTED) {
		return;
	}

	for (index = 0U; index < service->compiled_count; ++index) {
		int64_t candidate_utc_minute;
		int ret;

		if (!service->entries[index].in_use || !service->entries[index].enabled) {
			continue;
		}

		ret = scheduler_runtime_entry_next_utc_minute(
			&service->entries[index],
			service->baseline_utc_minute,
			&candidate_utc_minute);
		if (ret != 0) {
			continue;
		}

		if (!found || candidate_utc_minute < earliest_utc_minute) {
			found = true;
			earliest_utc_minute = candidate_utc_minute;
			service->status.next_run.available = true;
			service->status.next_run.normalized_utc_minute =
				candidate_utc_minute;
			memcpy(service->status.next_run.schedule_id,
			       service->entries[index].schedule_id,
			       sizeof(service->status.next_run.schedule_id));
			memcpy(service->status.next_run.action_id,
			       service->entries[index].action_id,
			       sizeof(service->status.next_run.action_id));
		}
	}
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
	scheduler_set_last_result_locked(service,
		SCHEDULER_LAST_RESULT_SKIPPED_BASELINE_RESET,
		0,
		normalized_utc_minute,
		NULL,
		NULL);
	if (problem_code != SCHEDULER_PROBLEM_NONE) {
		scheduler_record_problem_locked(service, problem_code,
					       normalized_utc_minute,
					       0,
					       NULL,
					       NULL);
		scheduler_record_problem_locked(service,
					       SCHEDULER_PROBLEM_FUTURE_ONLY_BASELINE_APPLIED,
					       normalized_utc_minute,
					       0,
					       NULL,
					       NULL);
	}

	scheduler_refresh_automation_locked(service);
	scheduler_refresh_next_run_locked(service);
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

const char *scheduler_problem_code_text(enum scheduler_problem_code code)
{
	switch (code) {
	case SCHEDULER_PROBLEM_BACKWARD_UTC_CLOCK_JUMP:
		return "backward-utc-clock-jump";
	case SCHEDULER_PROBLEM_NORMALIZED_UTC_MINUTE_CORRECTED:
		return "normalized-utc-minute-corrected";
	case SCHEDULER_PROBLEM_FUTURE_ONLY_BASELINE_APPLIED:
		return "future-only-baseline-applied";
	case SCHEDULER_PROBLEM_TRUSTED_CLOCK_UNAVAILABLE:
		return "trusted-clock-unavailable";
	case SCHEDULER_PROBLEM_ACTION_DISPATCH_FAILED:
		return "action-dispatch-failed";
	case SCHEDULER_PROBLEM_NONE:
	default:
		return "none";
	}
}

int scheduler_cron_validate_expression(const char *expression)
{
	struct scheduler_cron_matcher cron;

	return scheduler_compile_expression(expression, &cron);
}

int scheduler_schedule_table_validate(
	const struct persisted_schedule_table *schedule_table,
	const struct persisted_action_catalog *actions)
{
	struct scheduler_runtime_entry entries[PERSISTED_SCHEDULE_MAX_COUNT];

	return scheduler_compile_schedule_table(schedule_table, actions, entries,
					      NULL, NULL);
}

int scheduler_service_reload(struct scheduler_service *service)
{
	struct scheduler_runtime_entry entries[PERSISTED_SCHEDULE_MAX_COUNT];
	uint32_t compiled_count;
	uint32_t enabled_count;
	int ret;

	if (service == NULL || service->app_context == NULL) {
		return -EINVAL;
	}

	ret = scheduler_compile_schedule_table(
		&service->app_context->persisted_config.schedule,
		&service->app_context->persisted_config.actions,
		entries,
		&compiled_count,
		&enabled_count);
	if (ret != 0) {
		return ret;
	}

	k_mutex_lock(&service->lock, K_FOREVER);
	memcpy(service->entries, entries, sizeof(service->entries));
	service->compiled_count = compiled_count;
	service->status.schedule_count = compiled_count;
	service->status.enabled_schedule_count = enabled_count;
	scheduler_refresh_counts_locked(service);
	scheduler_refresh_automation_locked(service);
	scheduler_refresh_next_run_locked(service);
	k_mutex_unlock(&service->lock);

	return 0;
}

int scheduler_service_start(struct scheduler_service *service)
{
	int64_t utc_epoch_seconds;
	int ret;

	if (service == NULL || service->app_context == NULL) {
		return -EINVAL;
	}

	service->runtime_started = true;

	ret = scheduler_service_sync_clock(service, &utc_epoch_seconds);
	if (ret == 0) {
		ret = scheduler_service_acquire_trusted_time(service, utc_epoch_seconds);
		if (ret == 0) {
			LOG_INF("Trusted UTC clock acquired via SNTP and CLOCK_REALTIME");
		}
		scheduler_service_schedule_runtime_work(service);
		return ret;
	}

	if (!scheduler_service_prior_clock_reset(service)) {
		LOG_WRN("Trusted UTC clock acquisition failed (%d); requesting one recovery reset",
			ret);
		recovery_manager_request_reset(&service->app_context->recovery,
					      RECOVERY_RESET_TRIGGER_TRUSTED_CLOCK_ACQUISITION,
					      ret);
	}

	LOG_WRN("Trusted UTC clock still unavailable after recovery reset; scheduling stays degraded");
	ret = scheduler_service_mark_clock_untrusted(
		service,
		SCHEDULER_DEGRADED_REASON_INITIAL_TRUST_ACQUISITION_FAILED);
	scheduler_service_schedule_runtime_work(service);
	return ret;
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
	scheduler_set_last_result_locked(service,
		SCHEDULER_LAST_RESULT_SKIPPED_UNTRUSTED_CLOCK,
		0,
		-1,
		NULL,
		NULL);
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

	normalized_utc_minute =
		scheduler_normalize_utc_minute(corrected_utc_epoch_seconds);

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

int scheduler_service_copy_snapshot(
	const struct scheduler_service *service,
	struct scheduler_runtime_status *status_out,
	struct scheduler_problem_record *problems_out,
	size_t problem_capacity,
	uint32_t *problem_count_out)
{
	struct scheduler_service *mutable_service = (struct scheduler_service *)service;
	uint32_t copied_count = 0U;
	uint32_t index;

	if (service == NULL || status_out == NULL) {
		return -EINVAL;
	}

	k_mutex_lock(&mutable_service->lock, K_FOREVER);
	*status_out = mutable_service->status;

	if (problems_out != NULL && problem_capacity > 0U) {
		copied_count = MIN(mutable_service->status.problem_count, problem_capacity);
		for (index = 0U; index < copied_count; ++index) {
			uint32_t problem_index =
				(mutable_service->problem_head + ARRAY_SIZE(mutable_service->problems) -
				 1U - index) %
				ARRAY_SIZE(mutable_service->problems);

			problems_out[index] = mutable_service->problems[problem_index];
		}
	}
	k_mutex_unlock(&mutable_service->lock);

	if (problem_count_out != NULL) {
		*problem_count_out = copied_count;
	}

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
	k_work_init_delayable(&service->runtime_work, scheduler_service_runtime_work_handler);

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

	LOG_INF("Scheduler service ready with UTC cron validation, output command conflict checks, legacy action compatibility, and trusted clock gating");
	return 0;
}
