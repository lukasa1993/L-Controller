#pragma once

#include <stdbool.h>

#include <zephyr/kernel.h>

#include "persistence/persistence_types.h"

struct app_context;

enum action_dispatch_source {
	ACTION_DISPATCH_SOURCE_NONE = 0,
	ACTION_DISPATCH_SOURCE_PANEL_MANUAL,
	ACTION_DISPATCH_SOURCE_SCHEDULER,
	ACTION_DISPATCH_SOURCE_SAFETY_POLICY,
};

enum action_dispatch_result_code {
	ACTION_DISPATCH_RESULT_OK = 0,
	ACTION_DISPATCH_RESULT_INVALID_REQUEST,
	ACTION_DISPATCH_RESULT_NOT_FOUND,
	ACTION_DISPATCH_RESULT_DISABLED,
	ACTION_DISPATCH_RESULT_UNAVAILABLE,
	ACTION_DISPATCH_RESULT_FAILED,
};

struct action_dispatch_result {
	enum action_dispatch_result_code code;
	enum action_dispatch_source source;
	int error_code;
	bool accepted;
	char action_id[PERSISTED_ACTION_ID_MAX_LEN];
	const char *detail;
};

struct action_dispatcher {
	struct app_context *app_context;
	struct k_mutex lock;
};

const char *action_dispatch_source_text(enum action_dispatch_source source);
const char *action_dispatch_result_text(enum action_dispatch_result_code code);
const char *action_dispatcher_builtin_relay_action_id(bool desired_state);

int action_dispatcher_init(struct action_dispatcher *dispatcher,
				   struct app_context *app_context);

int action_dispatcher_execute(struct action_dispatcher *dispatcher,
			      const char *action_id,
			      enum action_dispatch_source source,
			      struct action_dispatch_result *result);
