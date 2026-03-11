#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "persistence/persistence_types.h"

struct app_context;

#define RELAY_OUTPUT_KEY_RELAY0 "relay0"

struct relay_output_binding {
	const char *output_key;
	const char *display_name;
};

enum relay_status_source {
	RELAY_STATUS_SOURCE_NONE = 0,
	RELAY_STATUS_SOURCE_BOOT_POLICY,
	RELAY_STATUS_SOURCE_RECOVERY_POLICY,
	RELAY_STATUS_SOURCE_PANEL_MANUAL,
	RELAY_STATUS_SOURCE_SCHEDULER,
	RELAY_STATUS_SOURCE_SAFETY_POLICY,
};

struct relay_runtime_status {
	bool implemented;
	bool available;
	bool actual_state;
	bool desired_state;
	enum relay_status_source source;
	const char *safety_note;
};

struct relay_service {
	struct app_context *app_context;
	struct relay_runtime_status status;
};

size_t relay_output_binding_count(void);
const struct relay_output_binding *relay_output_binding_get(size_t index);
const struct relay_output_binding *relay_output_binding_find(const char *output_key);
bool relay_output_binding_is_valid(const char *output_key);
bool relay_command_is_valid(enum persisted_action_command command);
const char *relay_command_text(enum persisted_action_command command);
int relay_command_desired_state(enum persisted_action_command command, bool *desired_state);

int relay_service_init(struct relay_service *service, struct app_context *app_context);
const struct relay_runtime_status *relay_service_get_status(const struct relay_service *service);
int relay_service_apply_command(struct relay_service *service,
				bool desired_state,
				enum relay_status_source source);
int relay_service_apply_bound_command(struct relay_service *service,
				      const char *output_key,
				      enum persisted_action_command command,
				      enum relay_status_source source);
int relay_service_restore_status(struct relay_service *service,
				 const struct relay_runtime_status *status);
const char *relay_status_source_text(enum relay_status_source source);
