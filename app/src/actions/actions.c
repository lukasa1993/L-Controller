#include <errno.h>
#include <string.h>

#include <zephyr/logging/log.h>

#include "actions/actions.h"
#include "app/app_context.h"

LOG_MODULE_DECLARE(app, CONFIG_LOG_DEFAULT_LEVEL);

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

int action_dispatcher_init(struct action_dispatcher *dispatcher,
			   struct app_context *app_context)
{
	if (dispatcher == NULL || app_context == NULL) {
		return -EINVAL;
	}

	*dispatcher = (struct action_dispatcher){
		.app_context = app_context,
	};

	LOG_INF("Action dispatcher ready");
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
