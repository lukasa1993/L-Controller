#pragma once

#include <stdbool.h>

struct app_context;

enum relay_status_source {
	RELAY_STATUS_SOURCE_NONE = 0,
	RELAY_STATUS_SOURCE_BOOT_POLICY,
	RELAY_STATUS_SOURCE_RECOVERY_POLICY,
	RELAY_STATUS_SOURCE_COMMAND,
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

int relay_service_init(struct relay_service *service, struct app_context *app_context);
const struct relay_runtime_status *relay_service_get_status(const struct relay_service *service);
const char *relay_status_source_text(enum relay_status_source source);
