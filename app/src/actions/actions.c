#include <stdbool.h>
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

static void action_dispatch_catalog_copy(
	const struct persisted_action_catalog *catalog,
	struct persisted_action_catalog_save_request *request)
{
	uint32_t copy_count;

	memset(request, 0, sizeof(*request));
	if (catalog == NULL) {
		return;
	}

	copy_count = MIN(catalog->count, PERSISTED_ACTION_MAX_COUNT);
	request->count = catalog->count;
	memcpy(request->entries, catalog->entries,
	       sizeof(request->entries[0]) * copy_count);
}

static struct persisted_action *action_dispatch_catalog_find_mutable(
	struct persisted_action_catalog_save_request *request,
	const char *action_id)
{
	uint32_t max_count;
	uint32_t index;

	if (request == NULL || action_id == NULL) {
		return NULL;
	}

	max_count = MIN(request->count, PERSISTED_ACTION_MAX_COUNT);
	for (index = 0; index < max_count; ++index) {
		if (strcmp(request->entries[index].action_id, action_id) == 0) {
			return &request->entries[index];
		}
	}

	return NULL;
}

static int action_dispatch_catalog_ensure_builtin(
	struct persisted_action_catalog_save_request *request,
	const char *action_id,
	bool relay_on,
	bool *changed)
{
	struct persisted_action *entry;

	entry = action_dispatch_catalog_find_mutable(request, action_id);
	if (entry == NULL) {
		if (request->count >= PERSISTED_ACTION_MAX_COUNT) {
			return -ENOSPC;
		}

		entry = &request->entries[request->count++];
		memset(entry, 0, sizeof(*entry));
		strncpy(entry->action_id, action_id, sizeof(entry->action_id) - 1U);
		entry->enabled = true;
		entry->relay_on = relay_on;
		*changed = true;
		return 0;
	}

	if (!entry->enabled || entry->relay_on != relay_on) {
		entry->enabled = true;
		entry->relay_on = relay_on;
		*changed = true;
	}

	return 0;
}

static int action_dispatcher_ensure_builtin_actions(
	struct action_dispatcher *dispatcher)
{
	struct persisted_action_catalog_save_request save_request;
	struct app_context *app_context = dispatcher->app_context;
	bool changed = false;
	int ret;

	action_dispatch_catalog_copy(&app_context->persisted_config.actions,
				     &save_request);

	ret = action_dispatch_catalog_ensure_builtin(&save_request,
				     relay_builtin_off_action_id,
				     false,
				     &changed);
	if (ret != 0) {
		return ret;
	}

	ret = action_dispatch_catalog_ensure_builtin(&save_request,
				     relay_builtin_on_action_id,
				     true,
				     &changed);
	if (ret != 0) {
		return ret;
	}

	if (!changed) {
		return 0;
	}

	ret = persistence_store_save_actions(&app_context->persistence,
					     &app_context->persisted_config,
					     &save_request);
	if (ret != 0) {
		LOG_ERR("Failed to persist built-in relay actions: %d", ret);
		return ret;
	}

	LOG_INF("Seeded built-in relay actions count=%u",
		(unsigned int)app_context->persisted_config.actions.count);
	return 0;
}

static const struct persisted_action *action_dispatcher_find_action(
	const struct action_dispatcher *dispatcher,
	const char *action_id)
{
	const struct persisted_action_catalog *catalog;
	uint32_t max_count;
	uint32_t index;

	if (dispatcher == NULL || dispatcher->app_context == NULL || action_id == NULL) {
		return NULL;
	}

	catalog = &dispatcher->app_context->persisted_config.actions;
	max_count = MIN(catalog->count, PERSISTED_ACTION_MAX_COUNT);
	for (index = 0; index < max_count; ++index) {
		if (strcmp(catalog->entries[index].action_id, action_id) == 0) {
			return &catalog->entries[index];
		}
	}

	return NULL;
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

int action_dispatcher_init(struct action_dispatcher *dispatcher,
			   struct app_context *app_context)
{
	int ret;

	if (dispatcher == NULL || app_context == NULL) {
		return -EINVAL;
	}

	*dispatcher = (struct action_dispatcher){
		.app_context = app_context,
	};

	ret = action_dispatcher_ensure_builtin_actions(dispatcher);
	if (ret != 0) {
		return ret;
	}

	LOG_INF("Action dispatcher ready actions=%u",
		(unsigned int)app_context->persisted_config.actions.count);
	return 0;
}

int action_dispatcher_execute(struct action_dispatcher *dispatcher,
			      const char *action_id,
			      enum action_dispatch_source source,
			      struct action_dispatch_result *result)
{
	const struct relay_runtime_status *status;
	const struct persisted_action *action;
	struct persisted_relay_save_request save_request;
	struct relay_runtime_status previous_status;
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

	action = action_dispatcher_find_action(dispatcher, action_id);
	if (action == NULL) {
		action_dispatch_result_fail(result,
					ACTION_DISPATCH_RESULT_NOT_FOUND,
					0,
					"Action ID was not found");
		return 0;
	}

	if (!action->enabled) {
		action_dispatch_result_fail(result,
					ACTION_DISPATCH_RESULT_DISABLED,
					0,
					"Action is disabled");
		return 0;
	}

	status = relay_service_get_status(&dispatcher->app_context->relay);
	if (status == NULL || !status->implemented || !status->available) {
		action_dispatch_result_fail(result,
					ACTION_DISPATCH_RESULT_UNAVAILABLE,
					0,
					"Relay runtime is unavailable");
		return 0;
	}

	previous_status = *status;
	ret = relay_service_apply_command(&dispatcher->app_context->relay,
					     action->relay_on,
					     action_dispatcher_relay_source(source));
	if (ret != 0) {
		LOG_ERR("Failed to apply action %s via relay service: %d", action_id, ret);
		action_dispatch_result_fail(result,
					ACTION_DISPATCH_RESULT_FAILED,
					ret,
					"Relay actuation failed");
		return ret;
	}

	save_request = (struct persisted_relay_save_request){
		.last_desired_state = action->relay_on,
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
			return ret;
		}

		action_dispatch_result_fail(result,
					ACTION_DISPATCH_RESULT_FAILED,
					ret,
					"Desired-state persistence failed");
		return ret;
	}

	if (result != NULL) {
		result->code = ACTION_DISPATCH_RESULT_OK;
		result->error_code = 0;
		result->accepted = true;
		result->detail = "accepted";
	}

	LOG_INF("Executed action id=%s source=%s result=%s",
		action_id,
		action_dispatch_source_text(source),
		action_dispatch_result_text(ACTION_DISPATCH_RESULT_OK));
	return 0;
}
