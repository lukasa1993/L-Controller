#include <errno.h>
#include <string.h>

#include <zephyr/random/random.h>
#include <zephyr/sys/util.h>

#include "app/app_context.h"
#include "panel/panel_auth.h"

#define PANEL_AUTH_TOKEN_RANDOM_BYTES 16

static int panel_auth_generate_token(char *token, size_t token_len)
{
	static const char hex_digits[] = "0123456789abcdef";
	uint8_t random_bytes[PANEL_AUTH_TOKEN_RANDOM_BYTES];
	int ret;

	if (token == NULL || token_len < PANEL_AUTH_SESSION_TOKEN_LEN + 1) {
		return -EINVAL;
	}

	ret = sys_csrand_get(random_bytes, sizeof(random_bytes));
	if (ret != 0) {
		return ret;
	}

	for (size_t index = 0; index < sizeof(random_bytes); ++index) {
		token[index * 2] = hex_digits[random_bytes[index] >> 4];
		token[index * 2 + 1] = hex_digits[random_bytes[index] & 0x0fU];
	}

	token[PANEL_AUTH_SESSION_TOKEN_LEN] = '\0';
	return 0;
}

static void panel_auth_reset_sessions(struct panel_auth_service *service)
{
	memset(service->sessions, 0, sizeof(service->sessions));
}

static bool panel_auth_credentials_match(const struct panel_auth_service *service,
					 const char *username,
					 const char *password)
{
	const struct persisted_auth *auth;

	if (service == NULL || service->app_context == NULL || username == NULL || password == NULL) {
		return false;
	}

	auth = &service->app_context->persisted_config.auth;
	return strncmp(auth->username, username, sizeof(auth->username)) == 0 &&
	       strncmp(auth->password, password, sizeof(auth->password)) == 0;
}

static struct panel_auth_session *panel_auth_find_session(struct panel_auth_service *service,
						 const char *token)
{
	for (size_t index = 0; index < ARRAY_SIZE(service->sessions); ++index) {
		struct panel_auth_session *session = &service->sessions[index];

		if (session->in_use && strncmp(session->token, token, sizeof(session->token)) == 0) {
			return session;
		}
	}

	return NULL;
}

static struct panel_auth_session *panel_auth_find_free_session(struct panel_auth_service *service)
{
	for (size_t index = 0; index < ARRAY_SIZE(service->sessions); ++index) {
		if (!service->sessions[index].in_use) {
			return &service->sessions[index];
		}
	}

	return NULL;
}

int panel_auth_service_init(struct panel_auth_service *service,
			    struct app_context *app_context)
{
	if (service == NULL || app_context == NULL) {
		return -EINVAL;
	}

	memset(service, 0, sizeof(*service));
	service->app_context = app_context;
	k_mutex_init(&service->lock);
	panel_auth_reset_sessions(service);
	return 0;
}

int panel_auth_service_login(struct panel_auth_service *service,
			     const char *username,
			     const char *password,
			     struct panel_auth_login_result *result)
{
	struct panel_auth_session *session;
	int64_t now_ms;
	int64_t remaining_ms;
	int ret;

	if (service == NULL || username == NULL || password == NULL || result == NULL) {
		return -EINVAL;
	}

	memset(result, 0, sizeof(*result));
	now_ms = k_uptime_get();

	k_mutex_lock(&service->lock, K_FOREVER);

	remaining_ms = service->cooldown_until_ms - now_ms;
	if (remaining_ms > 0) {
		result->status = PANEL_AUTH_LOGIN_STATUS_COOLDOWN;
		result->retry_after_ms = remaining_ms > INT32_MAX ? INT32_MAX : (int32_t)remaining_ms;
		k_mutex_unlock(&service->lock);
		return 0;
	}

	if (!panel_auth_credentials_match(service, username, password)) {
		service->consecutive_failures++;
		if (service->consecutive_failures >= service->app_context->config.panel.login_failure_limit) {
			service->consecutive_failures = 0;
			service->cooldown_until_ms =
				now_ms + service->app_context->config.panel.login_cooldown_ms;
			result->status = PANEL_AUTH_LOGIN_STATUS_COOLDOWN;
			result->retry_after_ms = service->app_context->config.panel.login_cooldown_ms;
		} else {
			result->status = PANEL_AUTH_LOGIN_STATUS_INVALID_CREDENTIALS;
		}
		k_mutex_unlock(&service->lock);
		return 0;
	}

	session = panel_auth_find_free_session(service);
	if (session == NULL) {
		result->status = PANEL_AUTH_LOGIN_STATUS_CAPACITY_REACHED;
		k_mutex_unlock(&service->lock);
		return 0;
	}

	ret = panel_auth_generate_token(session->token, sizeof(session->token));
	if (ret != 0) {
		k_mutex_unlock(&service->lock);
		return ret;
	}

	session->in_use = true;
	session->created_at_ms = now_ms;
	session->last_seen_at_ms = now_ms;
	service->consecutive_failures = 0;
	service->cooldown_until_ms = 0;
	result->status = PANEL_AUTH_LOGIN_STATUS_OK;
	memcpy(result->session_token, session->token, sizeof(result->session_token));

	k_mutex_unlock(&service->lock);
	return 0;
}

bool panel_auth_service_session_active(struct panel_auth_service *service,
				       const char *token)
{
	struct panel_auth_session *session;
	bool active = false;

	if (service == NULL || token == NULL || token[0] == '\0') {
		return false;
	}

	k_mutex_lock(&service->lock, K_FOREVER);
	session = panel_auth_find_session(service, token);
	if (session != NULL) {
		session->last_seen_at_ms = k_uptime_get();
		active = true;
	}
	k_mutex_unlock(&service->lock);

	return active;
}

bool panel_auth_service_logout(struct panel_auth_service *service,
			       const char *token)
{
	struct panel_auth_session *session;
	bool removed = false;

	if (service == NULL || token == NULL || token[0] == '\0') {
		return false;
	}

	k_mutex_lock(&service->lock, K_FOREVER);
	session = panel_auth_find_session(service, token);
	if (session != NULL) {
		memset(session, 0, sizeof(*session));
		removed = true;
	}
	k_mutex_unlock(&service->lock);

	return removed;
}
