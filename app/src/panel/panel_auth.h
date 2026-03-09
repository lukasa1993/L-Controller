#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <zephyr/kernel.h>

#include "app/app_config.h"

struct app_context;

#define PANEL_AUTH_SESSION_TOKEN_LEN 32

enum panel_auth_login_status {
	PANEL_AUTH_LOGIN_STATUS_OK = 0,
	PANEL_AUTH_LOGIN_STATUS_INVALID_CREDENTIALS,
	PANEL_AUTH_LOGIN_STATUS_COOLDOWN,
	PANEL_AUTH_LOGIN_STATUS_CAPACITY_REACHED,
};

struct panel_auth_session {
	bool in_use;
	char token[PANEL_AUTH_SESSION_TOKEN_LEN + 1];
	int64_t created_at_ms;
	int64_t last_seen_at_ms;
};

struct panel_auth_login_result {
	enum panel_auth_login_status status;
	char session_token[PANEL_AUTH_SESSION_TOKEN_LEN + 1];
	int32_t retry_after_ms;
};

struct panel_auth_service {
	struct app_context *app_context;
	struct k_mutex lock;
	uint32_t consecutive_failures;
	int64_t cooldown_until_ms;
	struct panel_auth_session sessions[APP_PANEL_MAX_SESSIONS];
};

int panel_auth_service_init(struct panel_auth_service *service,
			    struct app_context *app_context);
int panel_auth_service_login(struct panel_auth_service *service,
			     const char *username,
			     const char *password,
			     struct panel_auth_login_result *result);
bool panel_auth_service_session_active(struct panel_auth_service *service,
				       const char *token);
bool panel_auth_service_logout(struct panel_auth_service *service,
			       const char *token);
