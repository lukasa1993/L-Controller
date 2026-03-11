#pragma once

#include <stdbool.h>
#include <stddef.h>

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

enum action_catalog_state {
	ACTION_CATALOG_STATE_READY = 0,
	ACTION_CATALOG_STATE_DISABLED,
	ACTION_CATALOG_STATE_NEEDS_ATTENTION,
};

const char *action_dispatch_source_text(enum action_dispatch_source source);
const char *action_dispatch_result_text(enum action_dispatch_result_code code);
bool action_dispatcher_action_id_is_operator_safe(const char *action_id);
bool action_dispatcher_action_id_is_legacy_builtin(const char *action_id);
bool action_dispatcher_action_record_valid(const struct persisted_action *action,
					   bool allow_legacy_action_id);
enum action_catalog_state action_dispatcher_action_state(
	const struct persisted_action *action);
const char *action_dispatcher_action_state_text(enum action_catalog_state state);
int action_dispatcher_generate_action_id(
	const struct persisted_action_catalog *catalog,
	const char *display_name,
	const char *existing_action_id,
	char *action_id,
	size_t action_id_len);
bool action_dispatcher_catalog_has_action(
	const struct persisted_action_catalog *catalog,
	const char *action_id,
	bool include_legacy_compat);
int action_dispatcher_resolve_action(
	const struct persisted_action_catalog *catalog,
	const char *action_id,
	bool include_legacy_compat,
	struct persisted_action *action_out);
const char *action_dispatcher_builtin_relay_action_id(bool desired_state);
const char *action_dispatcher_public_action_key(const char *action_id);
const char *action_dispatcher_public_action_label(const char *action_id);
const char *action_dispatcher_action_id_from_public_key(const char *public_key);

int action_dispatcher_init(struct action_dispatcher *dispatcher,
				   struct app_context *app_context);

int action_dispatcher_execute(struct action_dispatcher *dispatcher,
			      const char *action_id,
			      enum action_dispatch_source source,
			      struct action_dispatch_result *result);
