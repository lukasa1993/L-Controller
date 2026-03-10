#include <stdbool.h>
#include <errno.h>
#include <string.h>

#include <zephyr/logging/log.h>

#include "actions/actions.h"
#include "app/app_context.h"
#include "persistence/persistence.h"

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
	action_dispatch_result_reset(result, action_id, source);

	if (dispatcher == NULL || dispatcher->app_context == NULL || action_id == NULL ||
	    action_id[0] == '\0') {
		if (result != NULL) {
			result->code = ACTION_DISPATCH_RESULT_INVALID_REQUEST;
			result->detail = "Action request is invalid";
		}

		return -EINVAL;
	}

	if (result != NULL) {
		result->detail = "Action execution path not implemented yet";
		result->code = ACTION_DISPATCH_RESULT_UNAVAILABLE;
	}

	return -ENOTSUP;
}
