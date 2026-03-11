#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include "actions/actions.h"
#include "app/app_context.h"
#include "persistence/persistence.h"
#include "relay/relay.h"

LOG_MODULE_DECLARE(app, CONFIG_LOG_DEFAULT_LEVEL);

static const char relay_builtin_off_action_id[] = "relay0.off";
static const char relay_builtin_on_action_id[] = "relay0.on";
static const char relay_public_off_action_key[] = "relay-off";
static const char relay_public_on_action_key[] = "relay-on";
static const char relay_public_off_action_label[] = "Relay Off";
static const char relay_public_on_action_label[] = "Relay On";
static const char action_id_fallback_prefix[] = "action";

static bool action_dispatcher_ascii_is_alpha(char value)
{
	return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z');
}

static bool action_dispatcher_ascii_is_digit(char value)
{
	return value >= '0' && value <= '9';
}

static bool action_dispatcher_ascii_is_alnum(char value)
{
	return action_dispatcher_ascii_is_alpha(value) ||
	       action_dispatcher_ascii_is_digit(value);
}

static char action_dispatcher_ascii_to_lower(char value)
{
	if (value >= 'A' && value <= 'Z') {
		return (char)(value - 'A' + 'a');
	}

	return value;
}

static bool action_dispatcher_is_separator(char value)
{
	switch (value) {
	case ' ':
	case '-':
	case '_':
	case '.':
	case '/':
		return true;
	default:
		return false;
	}
}

bool action_dispatcher_action_id_is_legacy_builtin(const char *action_id)
{
	if (action_id == NULL || action_id[0] == '\0') {
		return false;
	}

	return strcmp(action_id, relay_builtin_on_action_id) == 0 ||
	       strcmp(action_id, relay_builtin_off_action_id) == 0;
}

static bool action_dispatcher_copy_c_string(char *dst, size_t dst_len, const char *src)
{
	size_t src_len;

	if (dst == NULL || src == NULL || dst_len == 0U) {
		return false;
	}

	src_len = strlen(src);
	if (src_len >= dst_len) {
		return false;
	}

	memcpy(dst, src, src_len + 1U);
	return true;
}

static bool action_dispatcher_c_string_is_non_empty(const char *value, size_t value_len)
{
	return value != NULL && value[0] != '\0' && memchr(value, '\0', value_len) != NULL;
}

static const struct persisted_action *action_dispatcher_find_catalog_action(
	const struct persisted_action_catalog *catalog,
	const char *action_id)
{
	uint32_t max_count;
	uint32_t index;

	if (catalog == NULL || action_id == NULL || action_id[0] == '\0') {
		return NULL;
	}

	max_count = MIN(catalog->count, PERSISTED_ACTION_MAX_COUNT);
	for (index = 0U; index < max_count; ++index) {
		if (strcmp(catalog->entries[index].action_id, action_id) == 0) {
			return &catalog->entries[index];
		}
	}

	return NULL;
}

static void action_dispatcher_fill_action_record(
	struct persisted_action *action,
	const char *action_id,
	const char *display_name,
	const char *output_key,
	bool enabled,
	enum persisted_action_command command)
{
	if (action == NULL) {
		return;
	}

	memset(action, 0, sizeof(*action));
	(void)action_dispatcher_copy_c_string(action->action_id,
					    sizeof(action->action_id),
					    action_id);
	(void)action_dispatcher_copy_c_string(action->display_name,
					    sizeof(action->display_name),
					    display_name);
	(void)action_dispatcher_copy_c_string(action->output_key,
					    sizeof(action->output_key),
					    output_key);
	action->enabled = enabled;
	action->type = PERSISTED_ACTION_TYPE_RELAY_COMMAND;
	action->command = command;
}

static int action_dispatcher_build_legacy_action(
	const char *action_id,
	struct persisted_action *action_out)
{
	if (action_out == NULL || !action_dispatcher_action_id_is_legacy_builtin(action_id)) {
		return -EINVAL;
	}

	if (strcmp(action_id, relay_builtin_on_action_id) == 0) {
		action_dispatcher_fill_action_record(action_out,
						    relay_builtin_on_action_id,
						    "Legacy Relay On",
						    RELAY_OUTPUT_KEY_RELAY0,
						    true,
						    PERSISTED_ACTION_COMMAND_RELAY_ON);
		return 0;
	}

	if (strcmp(action_id, relay_builtin_off_action_id) == 0) {
		action_dispatcher_fill_action_record(action_out,
						    relay_builtin_off_action_id,
						    "Legacy Relay Off",
						    RELAY_OUTPUT_KEY_RELAY0,
						    true,
						    PERSISTED_ACTION_COMMAND_RELAY_OFF);
		return 0;
	}

	return -ENOENT;
}

static enum relay_status_source action_dispatcher_relay_source(
	enum action_dispatch_source source)
{
	switch (source) {
	case ACTION_DISPATCH_SOURCE_PANEL_MANUAL:
		return RELAY_STATUS_SOURCE_PANEL_MANUAL;
	case ACTION_DISPATCH_SOURCE_SCHEDULER:
		return RELAY_STATUS_SOURCE_SCHEDULER;
	case ACTION_DISPATCH_SOURCE_SAFETY_POLICY:
		return RELAY_STATUS_SOURCE_SAFETY_POLICY;
	case ACTION_DISPATCH_SOURCE_NONE:
	default:
		return RELAY_STATUS_SOURCE_NONE;
	}
}

static void action_dispatch_result_fail(struct action_dispatch_result *result,
				       enum action_dispatch_result_code code,
				       int error_code,
				       const char *detail)
{
	if (result == NULL) {
		return;
	}

	result->code = code;
	result->error_code = error_code;
	result->accepted = false;
	result->detail = detail;
}

static void action_dispatch_result_reset(struct action_dispatch_result *result,
					 const char *action_id,
					 enum action_dispatch_source source)
{
	if (result == NULL) {
		return;
	}

	memset(result, 0, sizeof(*result));
	result->code = ACTION_DISPATCH_RESULT_FAILED;
	result->source = source;
	result->detail = "Dispatcher did not run";

	if (action_id != NULL) {
		strncpy(result->action_id, action_id, sizeof(result->action_id) - 1U);
	}
}

bool action_dispatcher_action_id_is_operator_safe(const char *action_id)
{
	size_t index;

	if (action_id == NULL || action_id[0] == '\0') {
		return false;
	}

	for (index = 0U; action_id[index] != '\0'; ++index) {
		const char value = action_id[index];

		if (value == '-') {
			if (index == 0U || action_id[index + 1U] == '\0' ||
			    action_id[index + 1U] == '-') {
				return false;
			}

			continue;
		}

		if (!action_dispatcher_ascii_is_digit(value) &&
		    !(value >= 'a' && value <= 'z')) {
			return false;
		}
	}

	return true;
}

bool action_dispatcher_action_record_valid(const struct persisted_action *action,
					   bool allow_legacy_action_id)
{
	if (action == NULL ||
	    !action_dispatcher_c_string_is_non_empty(action->action_id,
						     sizeof(action->action_id)) ||
	    !action_dispatcher_c_string_is_non_empty(action->display_name,
						     sizeof(action->display_name)) ||
	    !action_dispatcher_c_string_is_non_empty(action->output_key,
						     sizeof(action->output_key))) {
		return false;
	}

	if (!action_dispatcher_action_id_is_operator_safe(action->action_id) &&
	    !(allow_legacy_action_id &&
	      action_dispatcher_action_id_is_legacy_builtin(action->action_id))) {
		return false;
	}

	if (action->type != PERSISTED_ACTION_TYPE_RELAY_COMMAND ||
	    !relay_output_binding_is_valid(action->output_key) ||
	    !relay_command_is_valid(action->command)) {
		return false;
	}

	return true;
}

enum action_catalog_state action_dispatcher_action_state(
	const struct persisted_action *action)
{
	if (!action_dispatcher_action_record_valid(action, false)) {
		return ACTION_CATALOG_STATE_NEEDS_ATTENTION;
	}

	if (!action->enabled) {
		return ACTION_CATALOG_STATE_DISABLED;
	}

	return ACTION_CATALOG_STATE_READY;
}

const char *action_dispatcher_action_state_text(enum action_catalog_state state)
{
	switch (state) {
	case ACTION_CATALOG_STATE_READY:
		return "Ready";
	case ACTION_CATALOG_STATE_DISABLED:
		return "Disabled";
	case ACTION_CATALOG_STATE_NEEDS_ATTENTION:
	default:
		return "Needs attention";
	}
}

static bool action_dispatcher_catalog_action_id_conflicts(
	const struct persisted_action_catalog *catalog,
	const char *action_id,
	const char *existing_action_id)
{
	const struct persisted_action *existing;

	existing = action_dispatcher_find_catalog_action(catalog, action_id);
	if (existing == NULL) {
		return false;
	}

	return existing_action_id == NULL || strcmp(existing->action_id, existing_action_id) != 0;
}

int action_dispatcher_generate_action_id(
	const struct persisted_action_catalog *catalog,
	const char *display_name,
	const char *existing_action_id,
	char *action_id,
	size_t action_id_len)
{
	char base[PERSISTED_ACTION_ID_MAX_LEN] = { 0 };
	size_t base_len = 0U;
	size_t index;
	bool previous_was_separator = true;
	uint32_t suffix;

	if (display_name == NULL || action_id == NULL || action_id_len == 0U) {
		return -EINVAL;
	}

	for (index = 0U; display_name[index] != '\0'; ++index) {
		const char normalized = action_dispatcher_ascii_to_lower(display_name[index]);

		if (action_dispatcher_ascii_is_alnum(normalized)) {
			if (base_len + 1U >= sizeof(base)) {
				break;
			}

			base[base_len++] = normalized;
			previous_was_separator = false;
			continue;
		}

		if (action_dispatcher_is_separator(display_name[index]) &&
		    !previous_was_separator &&
		    base_len + 1U < sizeof(base)) {
			base[base_len++] = '-';
			previous_was_separator = true;
		}
	}

	while (base_len > 0U && base[base_len - 1U] == '-') {
		base_len--;
	}
	base[base_len] = '\0';

	if (base_len == 0U &&
	    !action_dispatcher_copy_c_string(base, sizeof(base), action_id_fallback_prefix)) {
		return -ENAMETOOLONG;
	}

	if (!action_dispatcher_copy_c_string(action_id, action_id_len, base)) {
		return -ENAMETOOLONG;
	}

	if (!action_dispatcher_catalog_action_id_conflicts(catalog, action_id, existing_action_id)) {
		return 0;
	}

	for (suffix = 2U; suffix < 1000U; ++suffix) {
		char suffix_text[12];
		size_t suffix_len;
		size_t copy_len;

		if (snprintf(suffix_text, sizeof(suffix_text), "-%u", suffix) < 0) {
			return -EIO;
		}

		suffix_len = strlen(suffix_text);
		if (suffix_len + 1U >= action_id_len) {
			return -ENAMETOOLONG;
		}

		copy_len = strlen(base);
		if (copy_len + suffix_len >= action_id_len) {
			copy_len = action_id_len - suffix_len - 1U;
			while (copy_len > 0U && base[copy_len - 1U] == '-') {
				copy_len--;
			}
		}

		if (copy_len == 0U) {
			return -ENAMETOOLONG;
		}

		memcpy(action_id, base, copy_len);
		action_id[copy_len] = '\0';
		strcat(action_id, suffix_text);
		if (!action_dispatcher_catalog_action_id_conflicts(catalog,
								action_id,
								existing_action_id)) {
			return 0;
		}
	}

	return -EEXIST;
}

bool action_dispatcher_catalog_has_action(
	const struct persisted_action_catalog *catalog,
	const char *action_id,
	bool include_legacy_compat)
{
	struct persisted_action action;

	return action_dispatcher_resolve_action(catalog,
					     action_id,
					     include_legacy_compat,
					     &action) == 0;
}

int action_dispatcher_resolve_action(
	const struct persisted_action_catalog *catalog,
	const char *action_id,
	bool include_legacy_compat,
	struct persisted_action *action_out)
{
	const struct persisted_action *catalog_action;

	if (action_id == NULL || action_id[0] == '\0' || action_out == NULL) {
		return -EINVAL;
	}

	catalog_action = action_dispatcher_find_catalog_action(catalog, action_id);
	if (catalog_action != NULL) {
		*action_out = *catalog_action;
		return 0;
	}

	if (!include_legacy_compat || !action_dispatcher_action_id_is_legacy_builtin(action_id)) {
		return -ENOENT;
	}

	return action_dispatcher_build_legacy_action(action_id, action_out);
}

const char *action_dispatch_source_text(enum action_dispatch_source source)
{
	switch (source) {
	case ACTION_DISPATCH_SOURCE_PANEL_MANUAL:
		return "manual-panel";
	case ACTION_DISPATCH_SOURCE_SCHEDULER:
		return "scheduler";
	case ACTION_DISPATCH_SOURCE_SAFETY_POLICY:
		return "safety-policy";
	case ACTION_DISPATCH_SOURCE_NONE:
	default:
		return "none";
	}
}

const char *action_dispatch_result_text(enum action_dispatch_result_code code)
{
	switch (code) {
	case ACTION_DISPATCH_RESULT_OK:
		return "ok";
	case ACTION_DISPATCH_RESULT_INVALID_REQUEST:
		return "invalid-request";
	case ACTION_DISPATCH_RESULT_NOT_FOUND:
		return "not-found";
	case ACTION_DISPATCH_RESULT_DISABLED:
		return "disabled";
	case ACTION_DISPATCH_RESULT_UNAVAILABLE:
		return "unavailable";
	case ACTION_DISPATCH_RESULT_FAILED:
	default:
		return "failed";
	}
}

const char *action_dispatcher_builtin_relay_action_id(bool desired_state)
{
	return desired_state ? relay_builtin_on_action_id : relay_builtin_off_action_id;
}

const char *action_dispatcher_public_action_key(const char *action_id)
{
	if (action_id == NULL || action_id[0] == '\0') {
		return NULL;
	}

	if (strcmp(action_id, relay_builtin_on_action_id) == 0) {
		return relay_public_on_action_key;
	}

	if (strcmp(action_id, relay_builtin_off_action_id) == 0) {
		return relay_public_off_action_key;
	}

	return NULL;
}

const char *action_dispatcher_public_action_label(const char *action_id)
{
	if (action_id == NULL || action_id[0] == '\0') {
		return NULL;
	}

	if (strcmp(action_id, relay_builtin_on_action_id) == 0) {
		return relay_public_on_action_label;
	}

	if (strcmp(action_id, relay_builtin_off_action_id) == 0) {
		return relay_public_off_action_label;
	}

	return NULL;
}

const char *action_dispatcher_action_id_from_public_key(const char *public_key)
{
	if (public_key == NULL || public_key[0] == '\0') {
		return NULL;
	}

	if (strcmp(public_key, relay_public_on_action_key) == 0) {
		return relay_builtin_on_action_id;
	}

	if (strcmp(public_key, relay_public_off_action_key) == 0) {
		return relay_builtin_off_action_id;
	}

	return NULL;
}

int action_dispatcher_init(struct action_dispatcher *dispatcher,
			   struct app_context *app_context)
{
	if (dispatcher == NULL || app_context == NULL) {
		return -EINVAL;
	}

	*dispatcher = (struct action_dispatcher){
		.app_context = app_context,
	};
	k_mutex_init(&dispatcher->lock);

	LOG_INF("Action dispatcher ready configured_actions=%u legacy_compat=enabled",
		(unsigned int)app_context->persisted_config.actions.count);
	return 0;
}

int action_dispatcher_execute(struct action_dispatcher *dispatcher,
			      const char *action_id,
			      enum action_dispatch_source source,
			      struct action_dispatch_result *result)
{
	const struct relay_runtime_status *status;
	struct persisted_action action;
	struct persisted_relay_save_request save_request;
	struct relay_runtime_status previous_status;
	bool desired_state;
	bool release_lock = false;
	int ret;

	action_dispatch_result_reset(result, action_id, source);

	if (dispatcher == NULL || dispatcher->app_context == NULL || action_id == NULL ||
	    action_id[0] == '\0') {
		action_dispatch_result_fail(result,
					ACTION_DISPATCH_RESULT_INVALID_REQUEST,
					-EINVAL,
					"Action request is invalid");

		return -EINVAL;
	}

	if (strlen(action_id) >= PERSISTED_ACTION_ID_MAX_LEN) {
		action_dispatch_result_fail(result,
					ACTION_DISPATCH_RESULT_INVALID_REQUEST,
					-EINVAL,
					"Action ID exceeds the supported length");
		return -EINVAL;
	}

	k_mutex_lock(&dispatcher->lock, K_FOREVER);
	release_lock = true;

	ret = action_dispatcher_resolve_action(&dispatcher->app_context->persisted_config.actions,
					      action_id,
					      true,
					      &action);
	if (ret != 0) {
		action_dispatch_result_fail(result,
					ACTION_DISPATCH_RESULT_NOT_FOUND,
					0,
					"Action ID was not found");
		ret = 0;
		goto out;
	}

	if (!action_dispatcher_action_record_valid(&action, true)) {
		action_dispatch_result_fail(result,
					ACTION_DISPATCH_RESULT_INVALID_REQUEST,
					-EINVAL,
					"Action definition is invalid");
		ret = 0;
		goto out;
	}

	if (!action.enabled) {
		action_dispatch_result_fail(result,
					ACTION_DISPATCH_RESULT_DISABLED,
					0,
					"Action is disabled");
		ret = 0;
		goto out;
	}

	ret = relay_command_desired_state(action.command, &desired_state);
	if (ret != 0) {
		action_dispatch_result_fail(result,
					ACTION_DISPATCH_RESULT_INVALID_REQUEST,
					ret,
					"Action command is invalid");
		ret = 0;
		goto out;
	}

	status = relay_service_get_status(&dispatcher->app_context->relay);
	if (status == NULL || !status->implemented || !status->available) {
		action_dispatch_result_fail(result,
					ACTION_DISPATCH_RESULT_UNAVAILABLE,
					0,
					"Relay runtime is unavailable");
		ret = 0;
		goto out;
	}

	if (status->actual_state == desired_state &&
	    status->desired_state == desired_state &&
	    status->safety_note == NULL) {
		if (result != NULL) {
			result->code = ACTION_DISPATCH_RESULT_OK;
			result->error_code = 0;
			result->accepted = true;
			result->detail = "no-op";
		}

		LOG_INF("Skipped action id=%s source=%s result=no-op desired=%s",
			action_id,
			action_dispatch_source_text(source),
			desired_state ? "on" : "off");
		ret = 0;
		goto out;
	}

	previous_status = *status;
	ret = relay_service_apply_bound_command(&dispatcher->app_context->relay,
					       action.output_key,
					       action.command,
					       action_dispatcher_relay_source(source));
	if (ret != 0) {
		LOG_ERR("Failed to apply action %s via relay service: %d", action_id, ret);
		action_dispatch_result_fail(result,
					ACTION_DISPATCH_RESULT_FAILED,
					ret,
					"Relay actuation failed");
		goto out;
	}

	save_request = (struct persisted_relay_save_request){
		.last_desired_state = desired_state,
		.reboot_policy = dispatcher->app_context->persisted_config.relay.reboot_policy,
	};

	ret = persistence_store_save_relay(&dispatcher->app_context->persistence,
					   &dispatcher->app_context->persisted_config,
					   &save_request);
	if (ret != 0) {
		int rollback_ret = relay_service_restore_status(&dispatcher->app_context->relay,
						       &previous_status);

		LOG_ERR("Failed to persist desired relay state for %s: %d", action_id, ret);
		if (rollback_ret != 0) {
			LOG_ERR("Failed to roll back relay runtime after persistence error: %d",
				rollback_ret);
			action_dispatch_result_fail(result,
						ACTION_DISPATCH_RESULT_FAILED,
						ret,
						"Desired-state persistence failed and relay rollback failed");
			goto out;
		}

		action_dispatch_result_fail(result,
					ACTION_DISPATCH_RESULT_FAILED,
					ret,
					"Desired-state persistence failed");
		goto out;
	}

	if (result != NULL) {
		result->code = ACTION_DISPATCH_RESULT_OK;
		result->error_code = 0;
		result->accepted = true;
		result->detail = "accepted";
	}

	LOG_INF("Executed action id=%s output=%s command=%s source=%s result=%s",
		action_id,
		action.output_key,
		relay_command_text(action.command),
		action_dispatch_source_text(source),
		action_dispatch_result_text(ACTION_DISPATCH_RESULT_OK));
	ret = 0;

out:
	if (release_lock) {
		k_mutex_unlock(&dispatcher->lock);
	}

	return ret;
}
