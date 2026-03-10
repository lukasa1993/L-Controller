#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <zephyr/data/json.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/http/server.h>
#include <zephyr/net/http/service.h>
#include <zephyr/net/http/status.h>
#include <zephyr/sys/util.h>

#include "app/app_context.h"
#include "panel/panel_auth.h"
#include "panel/panel_http.h"
#include "panel/panel_status.h"

LOG_MODULE_REGISTER(panel_http, CONFIG_LOG_DEFAULT_LEVEL);

HTTP_SERVER_REGISTER_HEADER_CAPTURE(panel_http_cookie_header, "Cookie");

#define PANEL_SCHEDULE_ACTION_KEY_MAX_LEN 16

struct panel_auth_payload {
	char username[PERSISTED_AUTH_USERNAME_MAX_LEN];
	char password[PERSISTED_AUTH_PASSWORD_MAX_LEN];
};

static const struct json_obj_descr panel_auth_payload_descr[] = {
	JSON_OBJ_DESCR_PRIM(struct panel_auth_payload, username, JSON_TOK_STRING_BUF),
	JSON_OBJ_DESCR_PRIM(struct panel_auth_payload, password, JSON_TOK_STRING_BUF),
};

struct panel_login_route_context {
	struct panel_auth_service *auth_service;
	uint8_t request_body[160];
	size_t request_body_len;
	bool request_too_large;
	char set_cookie_header[96];
	char response_body[256];
	struct http_header headers[1];
};

struct panel_auth_route_context {
	struct panel_auth_service *auth_service;
	char set_cookie_header[96];
	char response_body[192];
	struct http_header headers[1];
};

struct panel_status_route_context {
	struct app_context *app_context;
	char set_cookie_header[96];
	char response_body[PANEL_STATUS_RESPONSE_BODY_LEN];
	struct http_header headers[1];
};

struct panel_relay_route_context {
	struct app_context *app_context;
	uint8_t request_body[64];
	size_t request_body_len;
	bool request_too_large;
	char set_cookie_header[96];
	char response_body[320];
	struct http_header headers[1];
};

struct panel_schedule_snapshot_route_context {
	struct app_context *app_context;
	char set_cookie_header[96];
	char response_body[PANEL_STATUS_RESPONSE_BODY_LEN];
	struct http_header headers[1];
};

struct panel_schedule_mutation_route_context {
	struct app_context *app_context;
	uint8_t request_body[256];
	size_t request_body_len;
	bool request_too_large;
	char set_cookie_header[96];
	char response_body[448];
	struct http_header headers[1];
};

struct panel_schedule_payload {
	char schedule_id[PERSISTED_SCHEDULE_ID_MAX_LEN];
	char cron_expression[PERSISTED_SCHEDULE_CRON_EXPRESSION_MAX_LEN];
	char action_key[PANEL_SCHEDULE_ACTION_KEY_MAX_LEN];
	bool enabled;
};

struct panel_schedule_enabled_payload {
	char schedule_id[PERSISTED_SCHEDULE_ID_MAX_LEN];
	bool enabled;
};

struct panel_schedule_delete_payload {
	char schedule_id[PERSISTED_SCHEDULE_ID_MAX_LEN];
};

static const uint8_t index_html_gz[] = {
#include "panel/index.html.gz.inc"
};

static const uint8_t main_js_gz[] = {
#include "panel/main.js.gz.inc"
};

static struct panel_login_route_context panel_auth_login_route_ctx;
static struct panel_auth_route_context panel_auth_logout_route_ctx;
static struct panel_auth_route_context panel_auth_session_route_ctx;
static struct panel_status_route_context panel_status_route_ctx;
static struct panel_relay_route_context panel_relay_route_ctx;
static struct panel_schedule_snapshot_route_context panel_schedule_snapshot_route_ctx;
static struct panel_schedule_mutation_route_context panel_schedule_create_route_ctx;
static struct panel_schedule_mutation_route_context panel_schedule_update_route_ctx;
static struct panel_schedule_mutation_route_context panel_schedule_delete_route_ctx;
static struct panel_schedule_mutation_route_context panel_schedule_enabled_route_ctx;

static struct http_resource_detail_static panel_shell_index_resource_detail = {
	.common = {
		.type = HTTP_RESOURCE_TYPE_STATIC,
		.bitmask_of_supported_http_methods = BIT(HTTP_GET),
		.content_encoding = "gzip",
		.content_type = "text/html",
	},
	.static_data = index_html_gz,
	.static_data_len = sizeof(index_html_gz),
};

static struct http_resource_detail_static panel_shell_main_js_resource_detail = {
	.common = {
		.type = HTTP_RESOURCE_TYPE_STATIC,
		.bitmask_of_supported_http_methods = BIT(HTTP_GET),
		.content_encoding = "gzip",
		.content_type = "text/javascript",
	},
	.static_data = main_js_gz,
	.static_data_len = sizeof(main_js_gz),
};

static int panel_auth_login_handler(struct http_client_ctx *client,
				      enum http_data_status status,
				      const struct http_request_ctx *request_ctx,
				      struct http_response_ctx *response_ctx,
				      void *user_data);
static int panel_auth_logout_handler(struct http_client_ctx *client,
				       enum http_data_status status,
				       const struct http_request_ctx *request_ctx,
				       struct http_response_ctx *response_ctx,
				       void *user_data);
static int panel_auth_session_handler(struct http_client_ctx *client,
				        enum http_data_status status,
				        const struct http_request_ctx *request_ctx,
				        struct http_response_ctx *response_ctx,
				        void *user_data);
static int panel_status_handler(struct http_client_ctx *client,
				 enum http_data_status status,
				 const struct http_request_ctx *request_ctx,
				 struct http_response_ctx *response_ctx,
				 void *user_data);
static int panel_relay_command_handler(struct http_client_ctx *client,
				       enum http_data_status status,
				       const struct http_request_ctx *request_ctx,
				       struct http_response_ctx *response_ctx,
				       void *user_data);
static int panel_schedule_snapshot_handler(struct http_client_ctx *client,
					 enum http_data_status status,
					 const struct http_request_ctx *request_ctx,
					 struct http_response_ctx *response_ctx,
					 void *user_data);
static int panel_schedule_create_handler(struct http_client_ctx *client,
				       enum http_data_status status,
				       const struct http_request_ctx *request_ctx,
				       struct http_response_ctx *response_ctx,
				       void *user_data);
static int panel_schedule_update_handler(struct http_client_ctx *client,
				       enum http_data_status status,
				       const struct http_request_ctx *request_ctx,
				       struct http_response_ctx *response_ctx,
				       void *user_data);
static int panel_schedule_delete_handler(struct http_client_ctx *client,
				       enum http_data_status status,
				       const struct http_request_ctx *request_ctx,
				       struct http_response_ctx *response_ctx,
				       void *user_data);
static int panel_schedule_enabled_handler(struct http_client_ctx *client,
					enum http_data_status status,
					const struct http_request_ctx *request_ctx,
					struct http_response_ctx *response_ctx,
					void *user_data);

static struct http_resource_detail_dynamic panel_auth_login_resource_detail = {
	.common = {
		.type = HTTP_RESOURCE_TYPE_DYNAMIC,
		.bitmask_of_supported_http_methods = BIT(HTTP_POST),
		.content_type = "application/json",
	},
	.cb = panel_auth_login_handler,
	.user_data = &panel_auth_login_route_ctx,
};

static struct http_resource_detail_dynamic panel_auth_logout_resource_detail = {
	.common = {
		.type = HTTP_RESOURCE_TYPE_DYNAMIC,
		.bitmask_of_supported_http_methods = BIT(HTTP_POST),
		.content_type = "application/json",
	},
	.cb = panel_auth_logout_handler,
	.user_data = &panel_auth_logout_route_ctx,
};

static struct http_resource_detail_dynamic panel_auth_session_resource_detail = {
	.common = {
		.type = HTTP_RESOURCE_TYPE_DYNAMIC,
		.bitmask_of_supported_http_methods = BIT(HTTP_GET),
		.content_type = "application/json",
	},
	.cb = panel_auth_session_handler,
	.user_data = &panel_auth_session_route_ctx,
};

static struct http_resource_detail_dynamic panel_status_resource_detail = {
	.common = {
		.type = HTTP_RESOURCE_TYPE_DYNAMIC,
		.bitmask_of_supported_http_methods = BIT(HTTP_GET),
		.content_type = "application/json",
	},
	.cb = panel_status_handler,
	.user_data = &panel_status_route_ctx,
};

static struct http_resource_detail_dynamic panel_relay_resource_detail = {
	.common = {
		.type = HTTP_RESOURCE_TYPE_DYNAMIC,
		.bitmask_of_supported_http_methods = BIT(HTTP_POST),
		.content_type = "application/json",
	},
	.cb = panel_relay_command_handler,
	.user_data = &panel_relay_route_ctx,
};

static struct http_resource_detail_dynamic panel_schedule_snapshot_resource_detail = {
	.common = {
		.type = HTTP_RESOURCE_TYPE_DYNAMIC,
		.bitmask_of_supported_http_methods = BIT(HTTP_GET),
		.content_type = "application/json",
	},
	.cb = panel_schedule_snapshot_handler,
	.user_data = &panel_schedule_snapshot_route_ctx,
};

static struct http_resource_detail_dynamic panel_schedule_create_resource_detail = {
	.common = {
		.type = HTTP_RESOURCE_TYPE_DYNAMIC,
		.bitmask_of_supported_http_methods = BIT(HTTP_POST),
		.content_type = "application/json",
	},
	.cb = panel_schedule_create_handler,
	.user_data = &panel_schedule_create_route_ctx,
};

static struct http_resource_detail_dynamic panel_schedule_update_resource_detail = {
	.common = {
		.type = HTTP_RESOURCE_TYPE_DYNAMIC,
		.bitmask_of_supported_http_methods = BIT(HTTP_POST),
		.content_type = "application/json",
	},
	.cb = panel_schedule_update_handler,
	.user_data = &panel_schedule_update_route_ctx,
};

static struct http_resource_detail_dynamic panel_schedule_delete_resource_detail = {
	.common = {
		.type = HTTP_RESOURCE_TYPE_DYNAMIC,
		.bitmask_of_supported_http_methods = BIT(HTTP_POST),
		.content_type = "application/json",
	},
	.cb = panel_schedule_delete_handler,
	.user_data = &panel_schedule_delete_route_ctx,
};

static struct http_resource_detail_dynamic panel_schedule_enabled_resource_detail = {
	.common = {
		.type = HTTP_RESOURCE_TYPE_DYNAMIC,
		.bitmask_of_supported_http_methods = BIT(HTTP_POST),
		.content_type = "application/json",
	},
	.cb = panel_schedule_enabled_handler,
	.user_data = &panel_schedule_enabled_route_ctx,
};

static uint16_t panel_http_service_port = APP_PANEL_PORT;

HTTP_SERVICE_DEFINE(panel_http_service, NULL, &panel_http_service_port,
		    CONFIG_HTTP_SERVER_MAX_CLIENTS, 4, NULL, NULL, NULL);

HTTP_RESOURCE_DEFINE(panel_shell_index_resource, panel_http_service, "/",
		     &panel_shell_index_resource_detail);
HTTP_RESOURCE_DEFINE(panel_shell_main_js_resource, panel_http_service, "/main.js",
		     &panel_shell_main_js_resource_detail);
HTTP_RESOURCE_DEFINE(panel_auth_login_resource, panel_http_service, "/api/auth/login",
		     &panel_auth_login_resource_detail);
HTTP_RESOURCE_DEFINE(panel_auth_logout_resource, panel_http_service, "/api/auth/logout",
		     &panel_auth_logout_resource_detail);
HTTP_RESOURCE_DEFINE(panel_auth_session_resource, panel_http_service, "/api/auth/session",
			     &panel_auth_session_resource_detail);
HTTP_RESOURCE_DEFINE(panel_status_resource, panel_http_service, "/api/status",
			     &panel_status_resource_detail);
HTTP_RESOURCE_DEFINE(panel_relay_resource, panel_http_service, "/api/relay/desired-state",
			     &panel_relay_resource_detail);
HTTP_RESOURCE_DEFINE(panel_schedule_snapshot_resource, panel_http_service,
			     "/api/schedules",
			     &panel_schedule_snapshot_resource_detail);
HTTP_RESOURCE_DEFINE(panel_schedule_create_resource, panel_http_service,
			     "/api/schedules/create",
			     &panel_schedule_create_resource_detail);
HTTP_RESOURCE_DEFINE(panel_schedule_update_resource, panel_http_service,
			     "/api/schedules/update",
			     &panel_schedule_update_resource_detail);
HTTP_RESOURCE_DEFINE(panel_schedule_delete_resource, panel_http_service,
			     "/api/schedules/delete",
			     &panel_schedule_delete_resource_detail);
HTTP_RESOURCE_DEFINE(panel_schedule_enabled_resource, panel_http_service,
			     "/api/schedules/set-enabled",
			     &panel_schedule_enabled_resource_detail);

static void panel_login_route_reset(struct panel_login_route_context *route_ctx)
{
	if (route_ctx == NULL) {
		return;
	}

	route_ctx->request_body_len = 0;
	route_ctx->request_too_large = false;
}

static void panel_relay_route_reset(struct panel_relay_route_context *route_ctx)
{
	if (route_ctx == NULL) {
		return;
	}

	route_ctx->request_body_len = 0;
	route_ctx->request_too_large = false;
}

static void panel_schedule_route_reset(
	struct panel_schedule_mutation_route_context *route_ctx)
{
	if (route_ctx == NULL) {
		return;
	}

	route_ctx->request_body_len = 0;
	route_ctx->request_too_large = false;
}

static int panel_http_write_json_response(struct http_response_ctx *response_ctx,
					 enum http_status status_code,
					 char *buffer,
					 size_t buffer_len,
					 const char *format,
					 ...)
{
	va_list args;
	int written;

	if (response_ctx == NULL || buffer == NULL || buffer_len == 0 || format == NULL) {
		return -EINVAL;
	}

	va_start(args, format);
	written = vsnprintf(buffer, buffer_len, format, args);
	va_end(args);
	if (written < 0 || (size_t)written >= buffer_len) {
		return -ENOMEM;
	}

	response_ctx->status = status_code;
	response_ctx->body = (const uint8_t *)buffer;
	response_ctx->body_len = (size_t)written;
	response_ctx->final_chunk = true;
	return 0;
}

static void panel_http_attach_cookie_header(struct http_response_ctx *response_ctx,
					  struct http_header *headers,
					  char *cookie_buffer,
					  size_t cookie_buffer_len,
					  const char *cookie_value)
{
	if (response_ctx == NULL || headers == NULL || cookie_buffer == NULL || cookie_value == NULL) {
		return;
	}

	snprintf(cookie_buffer, cookie_buffer_len, "%s", cookie_value);
	headers[0] = (struct http_header){
		.name = "Set-Cookie",
		.value = cookie_buffer,
	};
	response_ctx->headers = headers;
	response_ctx->header_count = 1;
}

static const char *panel_http_find_header(const struct http_request_ctx *request_ctx,
					  const char *header_name)
{
	if (request_ctx == NULL || header_name == NULL) {
		return NULL;
	}

	for (size_t index = 0; index < request_ctx->header_count; ++index) {
		const struct http_header *header = &request_ctx->headers[index];

		if (header->name != NULL && strcmp(header->name, header_name) == 0) {
			return header->value;
		}
	}

	return NULL;
}

static bool panel_http_extract_sid_cookie(const struct http_request_ctx *request_ctx,
					  char *token,
					  size_t token_len)
{
	const char *cookie_header;
	const char *cursor;
	const char *end;
	const char *value_start;
	size_t value_len;

	if (token == NULL || token_len == 0) {
		return false;
	}

	token[0] = '\0';
	cookie_header = panel_http_find_header(request_ctx, "Cookie");
	if (cookie_header == NULL) {
		return false;
	}

	cursor = cookie_header;
	while (*cursor != '\0') {
		while (*cursor == ' ' || *cursor == ';') {
			cursor++;
		}

		if (strncmp(cursor, "sid=", 4) == 0) {
			value_start = cursor + 4;
			end = value_start;
			while (*end != '\0' && *end != ';') {
				end++;
			}

			value_len = end - value_start;
			if (value_len == 0 || value_len >= token_len) {
				return false;
			}

			memcpy(token, value_start, value_len);
			token[value_len] = '\0';
			return true;
		}

		while (*cursor != '\0' && *cursor != ';') {
			cursor++;
		}
	}

	return false;
}

static bool panel_http_is_json_whitespace(char value)
{
	return value == ' ' || value == '\t' || value == '\n' || value == '\r';
}

static bool panel_http_parse_bool_field(const char *json,
				       const char *field_name,
				       bool *value)
{
	char needle[32];
	const char *cursor;
	int needle_len;

	if (json == NULL || field_name == NULL || value == NULL) {
		return false;
	}

	needle_len = snprintf(needle, sizeof(needle), "\"%s\"", field_name);
	if (needle_len < 0 || (size_t)needle_len >= sizeof(needle)) {
		return false;
	}

	cursor = strstr(json, needle);
	if (cursor == NULL) {
		return false;
	}

	cursor += needle_len;
	while (panel_http_is_json_whitespace(*cursor)) {
		cursor++;
	}

	if (*cursor != ':') {
		return false;
	}

	cursor++;
	while (panel_http_is_json_whitespace(*cursor)) {
		cursor++;
	}

	if (strncmp(cursor, "true", 4) == 0) {
		*value = true;
		return true;
	}

	if (strncmp(cursor, "false", 5) == 0) {
		*value = false;
		return true;
	}

	return false;
}

static bool panel_http_parse_string_field(const char *json,
					 const char *field_name,
					 char *value,
					 size_t value_len)
{
	char needle[48];
	const char *cursor;
	const char *end;
	int needle_len;
	size_t copied_len;

	if (json == NULL || field_name == NULL || value == NULL || value_len == 0U) {
		return false;
	}

	value[0] = '\0';
	needle_len = snprintf(needle, sizeof(needle), "\"%s\"", field_name);
	if (needle_len < 0 || (size_t)needle_len >= sizeof(needle)) {
		return false;
	}

	cursor = strstr(json, needle);
	if (cursor == NULL) {
		return false;
	}

	cursor += needle_len;
	while (panel_http_is_json_whitespace(*cursor)) {
		cursor++;
	}

	if (*cursor != ':') {
		return false;
	}

	cursor++;
	while (panel_http_is_json_whitespace(*cursor)) {
		cursor++;
	}

	if (*cursor != '"') {
		return false;
	}

	cursor++;
	end = cursor;
	while (*end != '\0' && *end != '"') {
		if (*end == '\\') {
			return false;
		}

		end++;
	}

	if (*end != '"') {
		return false;
	}

	copied_len = (size_t)(end - cursor);
	if (copied_len >= value_len) {
		return false;
	}

	memcpy(value, cursor, copied_len);
	value[copied_len] = '\0';
	return true;
}

static int panel_http_require_authenticated(
	const struct http_request_ctx *request_ctx,
	struct http_response_ctx *response_ctx,
	struct app_context *app_context,
	struct http_header *headers,
	char *set_cookie_header,
	size_t set_cookie_header_len,
	char *response_body,
	size_t response_body_len,
	bool *authenticated_out)
{
	char session_token[PANEL_AUTH_SESSION_TOKEN_LEN + 1];
	bool has_cookie;
	bool authenticated;

	if (request_ctx == NULL || response_ctx == NULL || app_context == NULL ||
	    headers == NULL || set_cookie_header == NULL || response_body == NULL ||
	    authenticated_out == NULL) {
		return -EINVAL;
	}

	*authenticated_out = false;
	has_cookie = panel_http_extract_sid_cookie(request_ctx, session_token, sizeof(session_token));
	authenticated = has_cookie &&
		panel_auth_service_session_active(&app_context->panel_auth, session_token);
	if (authenticated) {
		*authenticated_out = true;
		return 0;
	}

	if (has_cookie) {
		panel_http_attach_cookie_header(response_ctx,
					      headers,
					      set_cookie_header,
					      set_cookie_header_len,
					      "sid=; Path=/; HttpOnly; SameSite=Strict; Max-Age=0");
	}

	return panel_http_write_json_response(response_ctx,
				     HTTP_401_UNAUTHORIZED,
				     response_body,
				     response_body_len,
				     "{\"authenticated\":false}");
}

static bool panel_http_schedule_id_is_operator_safe(const char *schedule_id)
{
	size_t index;

	if (schedule_id == NULL || schedule_id[0] == '\0') {
		return false;
	}

	if (!isalnum((unsigned char)schedule_id[0])) {
		return false;
	}

	for (index = 0U; schedule_id[index] != '\0'; ++index) {
		const unsigned char current = (unsigned char)schedule_id[index];

		if (!(isalnum(current) || current == '-' || current == '_' ||
		      current == '.')) {
			return false;
		}
	}

	return true;
}

static void panel_http_copy_schedule_save_request(
	const struct persisted_schedule_table *schedule_table,
	struct persisted_schedule_table_save_request *request)
{
	uint32_t copy_count;

	memset(request, 0, sizeof(*request));
	if (schedule_table == NULL) {
		return;
	}

	copy_count = MIN(schedule_table->count, PERSISTED_SCHEDULE_MAX_COUNT);
	request->count = copy_count;
	memcpy(request->entries,
	       schedule_table->entries,
	       sizeof(request->entries[0]) * copy_count);
}

static int panel_http_find_schedule_index(
	const struct persisted_schedule_table_save_request *request,
	const char *schedule_id)
{
	uint32_t index;

	if (request == NULL || schedule_id == NULL) {
		return -1;
	}

	for (index = 0U; index < request->count; ++index) {
		if (strcmp(request->entries[index].schedule_id, schedule_id) == 0) {
			return (int)index;
		}
	}

	return -1;
}

static int panel_http_schedule_save_and_reload(
	struct app_context *app_context,
	const struct persisted_schedule_table_save_request *request)
{
	int ret;

	ret = persistence_store_save_schedule(&app_context->persistence,
					     &app_context->persisted_config,
					     request);
	if (ret != 0) {
		return ret;
	}

	ret = scheduler_service_reload(&app_context->scheduler);
	if (ret != 0) {
		LOG_ERR("Failed to reload scheduler after schedule save: %d", ret);
		return ret;
	}

	return 0;
}

static bool panel_http_parse_schedule_payload(const char *json,
				      struct panel_schedule_payload *payload)
{
	if (payload == NULL) {
		return false;
	}

	memset(payload, 0, sizeof(*payload));
	return panel_http_parse_string_field(json,
				      "scheduleId",
				      payload->schedule_id,
				      sizeof(payload->schedule_id)) &&
		panel_http_parse_string_field(json,
				      "cronExpression",
				      payload->cron_expression,
				      sizeof(payload->cron_expression)) &&
		panel_http_parse_string_field(json,
				      "actionKey",
				      payload->action_key,
				      sizeof(payload->action_key)) &&
		panel_http_parse_bool_field(json, "enabled", &payload->enabled);
}

static bool panel_http_parse_schedule_enabled_payload(
	const char *json,
	struct panel_schedule_enabled_payload *payload)
{
	if (payload == NULL) {
		return false;
	}

	memset(payload, 0, sizeof(*payload));
	return panel_http_parse_string_field(json,
				      "scheduleId",
				      payload->schedule_id,
				      sizeof(payload->schedule_id)) &&
		panel_http_parse_bool_field(json, "enabled", &payload->enabled);
}

static bool panel_http_parse_schedule_delete_payload(
	const char *json,
	struct panel_schedule_delete_payload *payload)
{
	if (payload == NULL) {
		return false;
	}

	memset(payload, 0, sizeof(*payload));
	return panel_http_parse_string_field(json,
				      "scheduleId",
				      payload->schedule_id,
				      sizeof(payload->schedule_id));
}

static int panel_http_write_schedule_error_response(
	struct http_response_ctx *response_ctx,
	enum http_status status_code,
	char *buffer,
	size_t buffer_len,
	const char *error,
	const char *detail,
	const char *field)
{
	if (field != NULL && field[0] != '\0') {
		return panel_http_write_json_response(response_ctx,
					status_code,
					buffer,
					buffer_len,
					"{\"accepted\":false,\"error\":\"%s\",\"field\":\"%s\",\"detail\":\"%s\"}",
					error,
					field,
					detail);
	}

	return panel_http_write_json_response(response_ctx,
				     status_code,
				     buffer,
				     buffer_len,
				     "{\"accepted\":false,\"error\":\"%s\",\"detail\":\"%s\"}",
				     error,
				     detail);
}

static int panel_http_write_schedule_success_response(
	struct http_response_ctx *response_ctx,
	char *buffer,
	size_t buffer_len,
	const char *schedule_id,
	const char *detail)
{
	return panel_http_write_json_response(response_ctx,
				     HTTP_200_OK,
				     buffer,
				     buffer_len,
				     "{\"accepted\":true,\"scheduleId\":\"%s\",\"detail\":\"%s\"}",
				     schedule_id,
				     detail);
}

static int panel_http_write_schedule_save_failure(
	struct http_response_ctx *response_ctx,
	char *buffer,
	size_t buffer_len,
	int error_code)
{
	switch (error_code) {
	case -EEXIST:
		return panel_http_write_schedule_error_response(response_ctx,
					      HTTP_409_CONFLICT,
					      buffer,
					      buffer_len,
					      "duplicate-schedule-id",
					      "Choose a different schedule ID before saving this schedule.",
					      "scheduleId");
	case -EADDRINUSE:
		return panel_http_write_schedule_error_response(response_ctx,
					      HTTP_409_CONFLICT,
					      buffer,
					      buffer_len,
					      "conflicting-schedule-minute",
					      "Enabled Relay On and Relay Off schedules cannot target the same UTC minute.",
					      "cronExpression");
	case -ENOSPC:
		return panel_http_write_schedule_error_response(response_ctx,
					      HTTP_409_CONFLICT,
					      buffer,
					      buffer_len,
					      "schedule-capacity-reached",
					      "The scheduler already stores the maximum number of schedules for this phase.",
					      "scheduleId");
	case -ENOENT:
		return panel_http_write_schedule_error_response(response_ctx,
					      HTTP_404_NOT_FOUND,
					      buffer,
					      buffer_len,
					      "schedule-not-found",
					      "The requested schedule was not found in persistent storage.",
					      "scheduleId");
	case -EINVAL:
		return panel_http_write_schedule_error_response(response_ctx,
					      HTTP_400_BAD_REQUEST,
					      buffer,
					      buffer_len,
					      "invalid-schedule-request",
					      "The schedule request was rejected before it could be saved.",
					      NULL);
	default:
		return panel_http_write_schedule_error_response(response_ctx,
					      HTTP_500_INTERNAL_SERVER_ERROR,
					      buffer,
					      buffer_len,
					      "schedule-save-failed",
					      "The device could not commit the schedule change. Check the console for details.",
					      NULL);
	}
}

static enum http_status panel_http_relay_status_code(const struct action_dispatch_result *result,
				    int dispatch_ret)
{
	if (dispatch_ret == -EINVAL || (result != NULL &&
		    result->code == ACTION_DISPATCH_RESULT_INVALID_REQUEST)) {
		return HTTP_400_BAD_REQUEST;
	}

	if (result != NULL && result->code == ACTION_DISPATCH_RESULT_UNAVAILABLE) {
		return HTTP_503_SERVICE_UNAVAILABLE;
	}

	if (result != NULL && result->code == ACTION_DISPATCH_RESULT_OK) {
		return HTTP_200_OK;
	}

	return HTTP_500_INTERNAL_SERVER_ERROR;
}

static int panel_auth_login_handler(struct http_client_ctx *client,
				      enum http_data_status status,
				      const struct http_request_ctx *request_ctx,
				      struct http_response_ctx *response_ctx,
				      void *user_data)
{
	struct panel_login_route_context *route_ctx = user_data;
	struct panel_auth_payload payload = {0};
	struct panel_auth_login_result login_result;
	const int expected_fields = BIT_MASK(ARRAY_SIZE(panel_auth_payload_descr));
	int ret;

	ARG_UNUSED(client);

	if (route_ctx == NULL || route_ctx->auth_service == NULL || request_ctx == NULL ||
	    response_ctx == NULL) {
		return -EINVAL;
	}

	if (status == HTTP_SERVER_DATA_ABORTED) {
		panel_login_route_reset(route_ctx);
		return 0;
	}

	if (request_ctx->data_len + route_ctx->request_body_len >= sizeof(route_ctx->request_body)) {
		route_ctx->request_too_large = true;
	} else if (request_ctx->data_len > 0) {
		memcpy(route_ctx->request_body + route_ctx->request_body_len,
		       request_ctx->data,
		       request_ctx->data_len);
		route_ctx->request_body_len += request_ctx->data_len;
	}

	if (status != HTTP_SERVER_DATA_FINAL) {
		return 0;
	}

	if (route_ctx->request_too_large) {
		ret = panel_http_write_json_response(response_ctx,
					    HTTP_400_BAD_REQUEST,
					    route_ctx->response_body,
					    sizeof(route_ctx->response_body),
					    "{\"authenticated\":false,\"error\":\"payload-too-large\"}");
		panel_login_route_reset(route_ctx);
		return ret;
	}

	route_ctx->request_body[route_ctx->request_body_len] = '\0';
	ret = json_obj_parse((char *)route_ctx->request_body,
			     route_ctx->request_body_len,
			     panel_auth_payload_descr,
			     ARRAY_SIZE(panel_auth_payload_descr),
			     &payload);
	if (ret != expected_fields || payload.username[0] == '\0' || payload.password[0] == '\0') {
		ret = panel_http_write_json_response(response_ctx,
					    HTTP_400_BAD_REQUEST,
					    route_ctx->response_body,
					    sizeof(route_ctx->response_body),
					    "{\"authenticated\":false,\"error\":\"invalid-request\"}");
		panel_login_route_reset(route_ctx);
		return ret;
	}

	ret = panel_auth_service_login(route_ctx->auth_service,
			      payload.username,
			      payload.password,
			      &login_result);
	if (ret != 0) {
		panel_login_route_reset(route_ctx);
		return ret;
	}

	switch (login_result.status) {
	case PANEL_AUTH_LOGIN_STATUS_OK:
		panel_http_attach_cookie_header(response_ctx,
					      route_ctx->headers,
					      route_ctx->set_cookie_header,
					      sizeof(route_ctx->set_cookie_header),
					      "sid=placeholder");
		snprintf(route_ctx->set_cookie_header,
			 sizeof(route_ctx->set_cookie_header),
			 "sid=%s; Path=/; HttpOnly; SameSite=Strict",
			 login_result.session_token);
		ret = panel_http_write_json_response(response_ctx,
					    HTTP_200_OK,
					    route_ctx->response_body,
					    sizeof(route_ctx->response_body),
					    "{\"authenticated\":true}");
		break;
	case PANEL_AUTH_LOGIN_STATUS_COOLDOWN:
		ret = panel_http_write_json_response(response_ctx,
					    HTTP_429_TOO_MANY_REQUESTS,
					    route_ctx->response_body,
					    sizeof(route_ctx->response_body),
					    "{\"authenticated\":false,\"error\":\"cooldown\",\"retryAfterMs\":%d}",
					    login_result.retry_after_ms);
		break;
	case PANEL_AUTH_LOGIN_STATUS_CAPACITY_REACHED:
		ret = panel_http_write_json_response(response_ctx,
					    HTTP_503_SERVICE_UNAVAILABLE,
					    route_ctx->response_body,
					    sizeof(route_ctx->response_body),
					    "{\"authenticated\":false,\"error\":\"session-capacity\"}");
		break;
	case PANEL_AUTH_LOGIN_STATUS_INVALID_CREDENTIALS:
	default:
		ret = panel_http_write_json_response(response_ctx,
					    HTTP_401_UNAUTHORIZED,
					    route_ctx->response_body,
					    sizeof(route_ctx->response_body),
					    "{\"authenticated\":false,\"error\":\"invalid-credentials\"}");
		break;
	}

	panel_login_route_reset(route_ctx);
	return ret;
}

static int panel_auth_logout_handler(struct http_client_ctx *client,
				       enum http_data_status status,
				       const struct http_request_ctx *request_ctx,
				       struct http_response_ctx *response_ctx,
				       void *user_data)
{
	struct panel_auth_route_context *route_ctx = user_data;
	char session_token[PANEL_AUTH_SESSION_TOKEN_LEN + 1];

	ARG_UNUSED(client);

	if (route_ctx == NULL || route_ctx->auth_service == NULL || request_ctx == NULL ||
	    response_ctx == NULL) {
		return -EINVAL;
	}

	if (status == HTTP_SERVER_DATA_ABORTED || status != HTTP_SERVER_DATA_FINAL) {
		return 0;
	}

	if (panel_http_extract_sid_cookie(request_ctx, session_token, sizeof(session_token))) {
		(void)panel_auth_service_logout(route_ctx->auth_service, session_token);
	}

	panel_http_attach_cookie_header(response_ctx,
				      route_ctx->headers,
				      route_ctx->set_cookie_header,
				      sizeof(route_ctx->set_cookie_header),
				      "sid=; Path=/; HttpOnly; SameSite=Strict; Max-Age=0");
	response_ctx->status = HTTP_204_NO_CONTENT;
	response_ctx->final_chunk = true;
	return 0;
}

static int panel_auth_session_handler(struct http_client_ctx *client,
				        enum http_data_status status,
				        const struct http_request_ctx *request_ctx,
				        struct http_response_ctx *response_ctx,
				        void *user_data)
{
	struct panel_auth_route_context *route_ctx = user_data;
	char session_token[PANEL_AUTH_SESSION_TOKEN_LEN + 1];
	bool has_cookie;
	bool authenticated;
	int ret;

	ARG_UNUSED(client);

	if (route_ctx == NULL || route_ctx->auth_service == NULL || request_ctx == NULL ||
	    response_ctx == NULL) {
		return -EINVAL;
	}

	if (status == HTTP_SERVER_DATA_ABORTED || status != HTTP_SERVER_DATA_FINAL) {
		return 0;
	}

	has_cookie = panel_http_extract_sid_cookie(request_ctx, session_token, sizeof(session_token));
	authenticated = has_cookie &&
		panel_auth_service_session_active(route_ctx->auth_service, session_token);
	if (!authenticated) {
		if (has_cookie) {
			panel_http_attach_cookie_header(response_ctx,
					      route_ctx->headers,
					      route_ctx->set_cookie_header,
					      sizeof(route_ctx->set_cookie_header),
					      "sid=; Path=/; HttpOnly; SameSite=Strict; Max-Age=0");
		}
		return panel_http_write_json_response(response_ctx,
					 HTTP_401_UNAUTHORIZED,
					 route_ctx->response_body,
					 sizeof(route_ctx->response_body),
					 "{\"authenticated\":false}");
	}

	ret = panel_http_write_json_response(response_ctx,
				    HTTP_200_OK,
				    route_ctx->response_body,
				    sizeof(route_ctx->response_body),
				    "{\"authenticated\":true,\"username\":\"%s\"}",
				    route_ctx->auth_service->app_context->persisted_config.auth.username);
	return ret;
}

static int panel_relay_command_handler(struct http_client_ctx *client,
				       enum http_data_status status,
				       const struct http_request_ctx *request_ctx,
				       struct http_response_ctx *response_ctx,
				       void *user_data)
{
	struct panel_relay_route_context *route_ctx = user_data;
	struct action_dispatch_result dispatch_result;
	char session_token[PANEL_AUTH_SESSION_TOKEN_LEN + 1];
	const char *action_id;
	bool has_cookie;
	bool authenticated;
	bool desired_state = false;
	bool parsed = false;
	enum http_status status_code;
	int ret;

	ARG_UNUSED(client);

	if (route_ctx == NULL || route_ctx->app_context == NULL || request_ctx == NULL ||
	    response_ctx == NULL) {
		return -EINVAL;
	}

	if (status == HTTP_SERVER_DATA_ABORTED) {
		panel_relay_route_reset(route_ctx);
		return 0;
	}

	if (request_ctx->data_len + route_ctx->request_body_len >= sizeof(route_ctx->request_body)) {
		route_ctx->request_too_large = true;
	} else if (request_ctx->data_len > 0) {
		memcpy(route_ctx->request_body + route_ctx->request_body_len,
		       request_ctx->data,
		       request_ctx->data_len);
		route_ctx->request_body_len += request_ctx->data_len;
	}

	if (status != HTTP_SERVER_DATA_FINAL) {
		return 0;
	}

	has_cookie = panel_http_extract_sid_cookie(request_ctx, session_token, sizeof(session_token));
	authenticated = has_cookie &&
		panel_auth_service_session_active(&route_ctx->app_context->panel_auth, session_token);
	if (!authenticated) {
		if (has_cookie) {
			panel_http_attach_cookie_header(response_ctx,
					      route_ctx->headers,
					      route_ctx->set_cookie_header,
					      sizeof(route_ctx->set_cookie_header),
					      "sid=; Path=/; HttpOnly; SameSite=Strict; Max-Age=0");
		}

		ret = panel_http_write_json_response(response_ctx,
					    HTTP_401_UNAUTHORIZED,
					    route_ctx->response_body,
					    sizeof(route_ctx->response_body),
					    "{\"authenticated\":false}");
		panel_relay_route_reset(route_ctx);
		return ret;
	}

	if (route_ctx->request_too_large) {
		ret = panel_http_write_json_response(response_ctx,
					    HTTP_400_BAD_REQUEST,
					    route_ctx->response_body,
					    sizeof(route_ctx->response_body),
					    "{\"accepted\":false,\"error\":\"payload-too-large\"}");
		panel_relay_route_reset(route_ctx);
		return ret;
	}

	route_ctx->request_body[route_ctx->request_body_len] = '\0';
	parsed = panel_http_parse_bool_field((char *)route_ctx->request_body,
					    "desiredState",
					    &desired_state) ||
		panel_http_parse_bool_field((char *)route_ctx->request_body,
					  "desired",
					  &desired_state);
	if (!parsed) {
		ret = panel_http_write_json_response(response_ctx,
					    HTTP_400_BAD_REQUEST,
					    route_ctx->response_body,
					    sizeof(route_ctx->response_body),
					    "{\"accepted\":false,\"error\":\"invalid-request\"}");
		panel_relay_route_reset(route_ctx);
		return ret;
	}

	action_id = action_dispatcher_builtin_relay_action_id(desired_state);
	ret = action_dispatcher_execute(&route_ctx->app_context->actions,
				      action_id,
				      ACTION_DISPATCH_SOURCE_PANEL_MANUAL,
				      &dispatch_result);
	status_code = panel_http_relay_status_code(&dispatch_result, ret);
	ret = panel_http_write_json_response(response_ctx,
				    status_code,
				    route_ctx->response_body,
				    sizeof(route_ctx->response_body),
				    "{\"accepted\":%s,\"desiredState\":%s,\"actionId\":\"%s\","
				    "\"result\":\"%s\",\"source\":\"%s\",\"detail\":\"%s\"}",
				    dispatch_result.accepted ? "true" : "false",
				    desired_state ? "true" : "false",
				    dispatch_result.action_id,
				    action_dispatch_result_text(dispatch_result.code),
				    action_dispatch_source_text(dispatch_result.source),
				    dispatch_result.detail != NULL ? dispatch_result.detail : "none");
	panel_relay_route_reset(route_ctx);
	return ret;
}

static int panel_schedule_snapshot_handler(struct http_client_ctx *client,
					 enum http_data_status status,
					 const struct http_request_ctx *request_ctx,
					 struct http_response_ctx *response_ctx,
					 void *user_data)
{
	struct panel_schedule_snapshot_route_context *route_ctx = user_data;
	bool authenticated = false;
	int ret;

	ARG_UNUSED(client);

	if (route_ctx == NULL || route_ctx->app_context == NULL || request_ctx == NULL ||
	    response_ctx == NULL) {
		return -EINVAL;
	}

	if (status == HTTP_SERVER_DATA_ABORTED || status != HTTP_SERVER_DATA_FINAL) {
		return 0;
	}

	ret = panel_http_require_authenticated(request_ctx,
					 response_ctx,
					 route_ctx->app_context,
					 route_ctx->headers,
					 route_ctx->set_cookie_header,
					 sizeof(route_ctx->set_cookie_header),
					 route_ctx->response_body,
					 sizeof(route_ctx->response_body),
					 &authenticated);
	if (ret != 0 || !authenticated) {
		return ret;
	}

	ret = panel_status_render_schedule_snapshot_json(route_ctx->app_context,
						 route_ctx->response_body,
						 sizeof(route_ctx->response_body));
	if (ret < 0) {
		return panel_http_write_json_response(response_ctx,
					 HTTP_500_INTERNAL_SERVER_ERROR,
					 route_ctx->response_body,
					 sizeof(route_ctx->response_body),
					 "{\"error\":\"schedule-render-failed\"}");
	}

	response_ctx->status = HTTP_200_OK;
	response_ctx->body = (const uint8_t *)route_ctx->response_body;
	response_ctx->body_len = (size_t)ret;
	response_ctx->final_chunk = true;
	return 0;
}

static int panel_schedule_create_handler(struct http_client_ctx *client,
				       enum http_data_status status,
				       const struct http_request_ctx *request_ctx,
				       struct http_response_ctx *response_ctx,
				       void *user_data)
{
	struct panel_schedule_mutation_route_context *route_ctx = user_data;
	struct persisted_schedule_table_save_request save_request;
	struct panel_schedule_payload payload;
	struct persisted_schedule *entry;
	const char *action_id;
	bool authenticated = false;
	int index;
	int ret;

	ARG_UNUSED(client);

	if (route_ctx == NULL || route_ctx->app_context == NULL || request_ctx == NULL ||
	    response_ctx == NULL) {
		return -EINVAL;
	}

	if (status == HTTP_SERVER_DATA_ABORTED) {
		panel_schedule_route_reset(route_ctx);
		return 0;
	}

	if (request_ctx->data_len + route_ctx->request_body_len >= sizeof(route_ctx->request_body)) {
		route_ctx->request_too_large = true;
	} else if (request_ctx->data_len > 0) {
		memcpy(route_ctx->request_body + route_ctx->request_body_len,
		       request_ctx->data,
		       request_ctx->data_len);
		route_ctx->request_body_len += request_ctx->data_len;
	}

	if (status != HTTP_SERVER_DATA_FINAL) {
		return 0;
	}

	ret = panel_http_require_authenticated(request_ctx,
					 response_ctx,
					 route_ctx->app_context,
					 route_ctx->headers,
					 route_ctx->set_cookie_header,
					 sizeof(route_ctx->set_cookie_header),
					 route_ctx->response_body,
					 sizeof(route_ctx->response_body),
					 &authenticated);
	if (ret != 0 || !authenticated) {
		panel_schedule_route_reset(route_ctx);
		return ret;
	}

	if (route_ctx->request_too_large) {
		ret = panel_http_write_schedule_error_response(response_ctx,
					      HTTP_400_BAD_REQUEST,
					      route_ctx->response_body,
					      sizeof(route_ctx->response_body),
					      "payload-too-large",
					      "Reduce the schedule payload size and try again.",
					      NULL);
		panel_schedule_route_reset(route_ctx);
		return ret;
	}

	route_ctx->request_body[route_ctx->request_body_len] = '\0';
	if (!panel_http_parse_schedule_payload((char *)route_ctx->request_body, &payload)) {
		ret = panel_http_write_schedule_error_response(response_ctx,
					      HTTP_400_BAD_REQUEST,
					      route_ctx->response_body,
					      sizeof(route_ctx->response_body),
					      "invalid-request",
					      "Provide scheduleId, cronExpression, actionKey, and enabled in the request body.",
					      NULL);
		panel_schedule_route_reset(route_ctx);
		return ret;
	}

	if (!panel_http_schedule_id_is_operator_safe(payload.schedule_id)) {
		ret = panel_http_write_schedule_error_response(response_ctx,
					      HTTP_400_BAD_REQUEST,
					      route_ctx->response_body,
					      sizeof(route_ctx->response_body),
					      "invalid-schedule-id",
					      "Schedule IDs must start with a letter or number and use only letters, numbers, dashes, underscores, or dots.",
					      "scheduleId");
		panel_schedule_route_reset(route_ctx);
		return ret;
	}

	action_id = action_dispatcher_action_id_from_public_key(payload.action_key);
	if (action_id == NULL) {
		ret = panel_http_write_schedule_error_response(response_ctx,
					      HTTP_400_BAD_REQUEST,
					      route_ctx->response_body,
					      sizeof(route_ctx->response_body),
					      "invalid-action-choice",
					      "Choose one of the supported relay actions before saving the schedule.",
					      "actionKey");
		panel_schedule_route_reset(route_ctx);
		return ret;
	}

	ret = scheduler_cron_validate_expression(payload.cron_expression);
	if (ret != 0) {
		ret = panel_http_write_schedule_error_response(response_ctx,
					      HTTP_400_BAD_REQUEST,
					      route_ctx->response_body,
					      sizeof(route_ctx->response_body),
					      "invalid-cron-expression",
					      "Use a five-field UTC cron expression with supported numeric ranges, lists, or step values.",
					      "cronExpression");
		panel_schedule_route_reset(route_ctx);
		return ret;
	}

	panel_http_copy_schedule_save_request(&route_ctx->app_context->persisted_config.schedule,
					      &save_request);
	index = panel_http_find_schedule_index(&save_request, payload.schedule_id);
	if (index >= 0) {
		ret = panel_http_write_schedule_save_failure(response_ctx,
					 route_ctx->response_body,
					 sizeof(route_ctx->response_body),
					 -EEXIST);
		panel_schedule_route_reset(route_ctx);
		return ret;
	}

	if (save_request.count >= PERSISTED_SCHEDULE_MAX_COUNT) {
		ret = panel_http_write_schedule_save_failure(response_ctx,
					 route_ctx->response_body,
					 sizeof(route_ctx->response_body),
					 -ENOSPC);
		panel_schedule_route_reset(route_ctx);
		return ret;
	}

	entry = &save_request.entries[save_request.count++];
	memset(entry, 0, sizeof(*entry));
	strncpy(entry->schedule_id, payload.schedule_id, sizeof(entry->schedule_id) - 1U);
	strncpy(entry->action_id, action_id, sizeof(entry->action_id) - 1U);
	entry->enabled = payload.enabled;
	strncpy(entry->cron_expression,
		payload.cron_expression,
		sizeof(entry->cron_expression) - 1U);

	ret = panel_http_schedule_save_and_reload(route_ctx->app_context, &save_request);
	if (ret != 0) {
		ret = panel_http_write_schedule_save_failure(response_ctx,
					 route_ctx->response_body,
					 sizeof(route_ctx->response_body),
					 ret);
		panel_schedule_route_reset(route_ctx);
		return ret;
	}

	ret = panel_http_write_schedule_success_response(response_ctx,
					 route_ctx->response_body,
					 sizeof(route_ctx->response_body),
					 payload.schedule_id,
					 "Schedule created.");
	panel_schedule_route_reset(route_ctx);
	return ret;
}

static int panel_schedule_update_handler(struct http_client_ctx *client,
				       enum http_data_status status,
				       const struct http_request_ctx *request_ctx,
				       struct http_response_ctx *response_ctx,
				       void *user_data)
{
	struct panel_schedule_mutation_route_context *route_ctx = user_data;
	struct persisted_schedule_table_save_request save_request;
	struct panel_schedule_payload payload;
	const char *action_id;
	bool authenticated = false;
	int index;
	int ret;

	ARG_UNUSED(client);

	if (route_ctx == NULL || route_ctx->app_context == NULL || request_ctx == NULL ||
	    response_ctx == NULL) {
		return -EINVAL;
	}

	if (status == HTTP_SERVER_DATA_ABORTED) {
		panel_schedule_route_reset(route_ctx);
		return 0;
	}

	if (request_ctx->data_len + route_ctx->request_body_len >= sizeof(route_ctx->request_body)) {
		route_ctx->request_too_large = true;
	} else if (request_ctx->data_len > 0) {
		memcpy(route_ctx->request_body + route_ctx->request_body_len,
		       request_ctx->data,
		       request_ctx->data_len);
		route_ctx->request_body_len += request_ctx->data_len;
	}

	if (status != HTTP_SERVER_DATA_FINAL) {
		return 0;
	}

	ret = panel_http_require_authenticated(request_ctx,
					 response_ctx,
					 route_ctx->app_context,
					 route_ctx->headers,
					 route_ctx->set_cookie_header,
					 sizeof(route_ctx->set_cookie_header),
					 route_ctx->response_body,
					 sizeof(route_ctx->response_body),
					 &authenticated);
	if (ret != 0 || !authenticated) {
		panel_schedule_route_reset(route_ctx);
		return ret;
	}

	if (route_ctx->request_too_large) {
		ret = panel_http_write_schedule_error_response(response_ctx,
					      HTTP_400_BAD_REQUEST,
					      route_ctx->response_body,
					      sizeof(route_ctx->response_body),
					      "payload-too-large",
					      "Reduce the schedule payload size and try again.",
					      NULL);
		panel_schedule_route_reset(route_ctx);
		return ret;
	}

	route_ctx->request_body[route_ctx->request_body_len] = '\0';
	if (!panel_http_parse_schedule_payload((char *)route_ctx->request_body, &payload)) {
		ret = panel_http_write_schedule_error_response(response_ctx,
					      HTTP_400_BAD_REQUEST,
					      route_ctx->response_body,
					      sizeof(route_ctx->response_body),
					      "invalid-request",
					      "Provide scheduleId, cronExpression, actionKey, and enabled in the request body.",
					      NULL);
		panel_schedule_route_reset(route_ctx);
		return ret;
	}

	if (!panel_http_schedule_id_is_operator_safe(payload.schedule_id)) {
		ret = panel_http_write_schedule_error_response(response_ctx,
					      HTTP_400_BAD_REQUEST,
					      route_ctx->response_body,
					      sizeof(route_ctx->response_body),
					      "invalid-schedule-id",
					      "Schedule IDs must start with a letter or number and use only letters, numbers, dashes, underscores, or dots.",
					      "scheduleId");
		panel_schedule_route_reset(route_ctx);
		return ret;
	}

	action_id = action_dispatcher_action_id_from_public_key(payload.action_key);
	if (action_id == NULL) {
		ret = panel_http_write_schedule_error_response(response_ctx,
					      HTTP_400_BAD_REQUEST,
					      route_ctx->response_body,
					      sizeof(route_ctx->response_body),
					      "invalid-action-choice",
					      "Choose one of the supported relay actions before saving the schedule.",
					      "actionKey");
		panel_schedule_route_reset(route_ctx);
		return ret;
	}

	ret = scheduler_cron_validate_expression(payload.cron_expression);
	if (ret != 0) {
		ret = panel_http_write_schedule_error_response(response_ctx,
					      HTTP_400_BAD_REQUEST,
					      route_ctx->response_body,
					      sizeof(route_ctx->response_body),
					      "invalid-cron-expression",
					      "Use a five-field UTC cron expression with supported numeric ranges, lists, or step values.",
					      "cronExpression");
		panel_schedule_route_reset(route_ctx);
		return ret;
	}

	panel_http_copy_schedule_save_request(&route_ctx->app_context->persisted_config.schedule,
					      &save_request);
	index = panel_http_find_schedule_index(&save_request, payload.schedule_id);
	if (index < 0) {
		ret = panel_http_write_schedule_save_failure(response_ctx,
					 route_ctx->response_body,
					 sizeof(route_ctx->response_body),
					 -ENOENT);
		panel_schedule_route_reset(route_ctx);
		return ret;
	}

	memset(&save_request.entries[index], 0, sizeof(save_request.entries[index]));
	strncpy(save_request.entries[index].schedule_id,
		payload.schedule_id,
		sizeof(save_request.entries[index].schedule_id) - 1U);
	strncpy(save_request.entries[index].action_id,
		action_id,
		sizeof(save_request.entries[index].action_id) - 1U);
	save_request.entries[index].enabled = payload.enabled;
	strncpy(save_request.entries[index].cron_expression,
		payload.cron_expression,
		sizeof(save_request.entries[index].cron_expression) - 1U);

	ret = panel_http_schedule_save_and_reload(route_ctx->app_context, &save_request);
	if (ret != 0) {
		ret = panel_http_write_schedule_save_failure(response_ctx,
					 route_ctx->response_body,
					 sizeof(route_ctx->response_body),
					 ret);
		panel_schedule_route_reset(route_ctx);
		return ret;
	}

	ret = panel_http_write_schedule_success_response(response_ctx,
					 route_ctx->response_body,
					 sizeof(route_ctx->response_body),
					 payload.schedule_id,
					 "Schedule updated.");
	panel_schedule_route_reset(route_ctx);
	return ret;
}

static int panel_schedule_delete_handler(struct http_client_ctx *client,
				       enum http_data_status status,
				       const struct http_request_ctx *request_ctx,
				       struct http_response_ctx *response_ctx,
				       void *user_data)
{
	struct panel_schedule_mutation_route_context *route_ctx = user_data;
	struct persisted_schedule_table_save_request save_request;
	struct panel_schedule_delete_payload payload;
	bool authenticated = false;
	int index;
	int ret;

	ARG_UNUSED(client);

	if (route_ctx == NULL || route_ctx->app_context == NULL || request_ctx == NULL ||
	    response_ctx == NULL) {
		return -EINVAL;
	}

	if (status == HTTP_SERVER_DATA_ABORTED) {
		panel_schedule_route_reset(route_ctx);
		return 0;
	}

	if (request_ctx->data_len + route_ctx->request_body_len >= sizeof(route_ctx->request_body)) {
		route_ctx->request_too_large = true;
	} else if (request_ctx->data_len > 0) {
		memcpy(route_ctx->request_body + route_ctx->request_body_len,
		       request_ctx->data,
		       request_ctx->data_len);
		route_ctx->request_body_len += request_ctx->data_len;
	}

	if (status != HTTP_SERVER_DATA_FINAL) {
		return 0;
	}

	ret = panel_http_require_authenticated(request_ctx,
					 response_ctx,
					 route_ctx->app_context,
					 route_ctx->headers,
					 route_ctx->set_cookie_header,
					 sizeof(route_ctx->set_cookie_header),
					 route_ctx->response_body,
					 sizeof(route_ctx->response_body),
					 &authenticated);
	if (ret != 0 || !authenticated) {
		panel_schedule_route_reset(route_ctx);
		return ret;
	}

	if (route_ctx->request_too_large) {
		ret = panel_http_write_schedule_error_response(response_ctx,
					      HTTP_400_BAD_REQUEST,
					      route_ctx->response_body,
					      sizeof(route_ctx->response_body),
					      "payload-too-large",
					      "Reduce the schedule payload size and try again.",
					      NULL);
		panel_schedule_route_reset(route_ctx);
		return ret;
	}

	route_ctx->request_body[route_ctx->request_body_len] = '\0';
	if (!panel_http_parse_schedule_delete_payload((char *)route_ctx->request_body, &payload)) {
		ret = panel_http_write_schedule_error_response(response_ctx,
					      HTTP_400_BAD_REQUEST,
					      route_ctx->response_body,
					      sizeof(route_ctx->response_body),
					      "invalid-request",
					      "Provide scheduleId in the delete request body.",
					      NULL);
		panel_schedule_route_reset(route_ctx);
		return ret;
	}

	if (!panel_http_schedule_id_is_operator_safe(payload.schedule_id)) {
		ret = panel_http_write_schedule_error_response(response_ctx,
					      HTTP_400_BAD_REQUEST,
					      route_ctx->response_body,
					      sizeof(route_ctx->response_body),
					      "invalid-schedule-id",
					      "Schedule IDs must start with a letter or number and use only letters, numbers, dashes, underscores, or dots.",
					      "scheduleId");
		panel_schedule_route_reset(route_ctx);
		return ret;
	}

	panel_http_copy_schedule_save_request(&route_ctx->app_context->persisted_config.schedule,
					      &save_request);
	index = panel_http_find_schedule_index(&save_request, payload.schedule_id);
	if (index < 0) {
		ret = panel_http_write_schedule_save_failure(response_ctx,
					 route_ctx->response_body,
					 sizeof(route_ctx->response_body),
					 -ENOENT);
		panel_schedule_route_reset(route_ctx);
		return ret;
	}

	if ((uint32_t)index + 1U < save_request.count) {
		memmove(&save_request.entries[index],
			&save_request.entries[index + 1],
			(sizeof(save_request.entries[0]) * (save_request.count - (uint32_t)index - 1U)));
	}
	memset(&save_request.entries[save_request.count - 1U], 0,
	       sizeof(save_request.entries[save_request.count - 1U]));
	save_request.count--;

	ret = panel_http_schedule_save_and_reload(route_ctx->app_context, &save_request);
	if (ret != 0) {
		ret = panel_http_write_schedule_save_failure(response_ctx,
					 route_ctx->response_body,
					 sizeof(route_ctx->response_body),
					 ret);
		panel_schedule_route_reset(route_ctx);
		return ret;
	}

	ret = panel_http_write_schedule_success_response(response_ctx,
					 route_ctx->response_body,
					 sizeof(route_ctx->response_body),
					 payload.schedule_id,
					 "Schedule deleted.");
	panel_schedule_route_reset(route_ctx);
	return ret;
}

static int panel_schedule_enabled_handler(struct http_client_ctx *client,
					enum http_data_status status,
					const struct http_request_ctx *request_ctx,
					struct http_response_ctx *response_ctx,
					void *user_data)
{
	struct panel_schedule_mutation_route_context *route_ctx = user_data;
	struct persisted_schedule_table_save_request save_request;
	struct panel_schedule_enabled_payload payload;
	const char *detail;
	bool authenticated = false;
	int index;
	int ret;

	ARG_UNUSED(client);

	if (route_ctx == NULL || route_ctx->app_context == NULL || request_ctx == NULL ||
	    response_ctx == NULL) {
		return -EINVAL;
	}

	if (status == HTTP_SERVER_DATA_ABORTED) {
		panel_schedule_route_reset(route_ctx);
		return 0;
	}

	if (request_ctx->data_len + route_ctx->request_body_len >= sizeof(route_ctx->request_body)) {
		route_ctx->request_too_large = true;
	} else if (request_ctx->data_len > 0) {
		memcpy(route_ctx->request_body + route_ctx->request_body_len,
		       request_ctx->data,
		       request_ctx->data_len);
		route_ctx->request_body_len += request_ctx->data_len;
	}

	if (status != HTTP_SERVER_DATA_FINAL) {
		return 0;
	}

	ret = panel_http_require_authenticated(request_ctx,
					 response_ctx,
					 route_ctx->app_context,
					 route_ctx->headers,
					 route_ctx->set_cookie_header,
					 sizeof(route_ctx->set_cookie_header),
					 route_ctx->response_body,
					 sizeof(route_ctx->response_body),
					 &authenticated);
	if (ret != 0 || !authenticated) {
		panel_schedule_route_reset(route_ctx);
		return ret;
	}

	if (route_ctx->request_too_large) {
		ret = panel_http_write_schedule_error_response(response_ctx,
					      HTTP_400_BAD_REQUEST,
					      route_ctx->response_body,
					      sizeof(route_ctx->response_body),
					      "payload-too-large",
					      "Reduce the schedule payload size and try again.",
					      NULL);
		panel_schedule_route_reset(route_ctx);
		return ret;
	}

	route_ctx->request_body[route_ctx->request_body_len] = '\0';
	if (!panel_http_parse_schedule_enabled_payload((char *)route_ctx->request_body,
					       &payload)) {
		ret = panel_http_write_schedule_error_response(response_ctx,
					      HTTP_400_BAD_REQUEST,
					      route_ctx->response_body,
					      sizeof(route_ctx->response_body),
					      "invalid-request",
					      "Provide scheduleId and enabled in the enable or disable request body.",
					      NULL);
		panel_schedule_route_reset(route_ctx);
		return ret;
	}

	if (!panel_http_schedule_id_is_operator_safe(payload.schedule_id)) {
		ret = panel_http_write_schedule_error_response(response_ctx,
					      HTTP_400_BAD_REQUEST,
					      route_ctx->response_body,
					      sizeof(route_ctx->response_body),
					      "invalid-schedule-id",
					      "Schedule IDs must start with a letter or number and use only letters, numbers, dashes, underscores, or dots.",
					      "scheduleId");
		panel_schedule_route_reset(route_ctx);
		return ret;
	}

	panel_http_copy_schedule_save_request(&route_ctx->app_context->persisted_config.schedule,
					      &save_request);
	index = panel_http_find_schedule_index(&save_request, payload.schedule_id);
	if (index < 0) {
		ret = panel_http_write_schedule_save_failure(response_ctx,
					 route_ctx->response_body,
					 sizeof(route_ctx->response_body),
					 -ENOENT);
		panel_schedule_route_reset(route_ctx);
		return ret;
	}

	save_request.entries[index].enabled = payload.enabled;
	ret = panel_http_schedule_save_and_reload(route_ctx->app_context, &save_request);
	if (ret != 0) {
		ret = panel_http_write_schedule_save_failure(response_ctx,
					 route_ctx->response_body,
					 sizeof(route_ctx->response_body),
					 ret);
		panel_schedule_route_reset(route_ctx);
		return ret;
	}

	detail = payload.enabled ? "Schedule enabled." : "Schedule disabled.";
	ret = panel_http_write_schedule_success_response(response_ctx,
					 route_ctx->response_body,
					 sizeof(route_ctx->response_body),
					 payload.schedule_id,
					 detail);
	panel_schedule_route_reset(route_ctx);
	return ret;
}

static int panel_status_handler(struct http_client_ctx *client,
				 enum http_data_status status,
				 const struct http_request_ctx *request_ctx,
				 struct http_response_ctx *response_ctx,
				 void *user_data)
{
	struct panel_status_route_context *route_ctx = user_data;
	char session_token[PANEL_AUTH_SESSION_TOKEN_LEN + 1];
	bool has_cookie;
	bool authenticated;
	int ret;

	ARG_UNUSED(client);

	if (route_ctx == NULL || route_ctx->app_context == NULL || request_ctx == NULL ||
	    response_ctx == NULL) {
		return -EINVAL;
	}

	if (status == HTTP_SERVER_DATA_ABORTED || status != HTTP_SERVER_DATA_FINAL) {
		return 0;
	}

	has_cookie = panel_http_extract_sid_cookie(request_ctx, session_token, sizeof(session_token));
	authenticated = has_cookie &&
		panel_auth_service_session_active(&route_ctx->app_context->panel_auth, session_token);
	if (!authenticated) {
		if (has_cookie) {
			panel_http_attach_cookie_header(response_ctx,
					      route_ctx->headers,
					      route_ctx->set_cookie_header,
					      sizeof(route_ctx->set_cookie_header),
					      "sid=; Path=/; HttpOnly; SameSite=Strict; Max-Age=0");
		}
		return panel_http_write_json_response(response_ctx,
					 HTTP_401_UNAUTHORIZED,
					 route_ctx->response_body,
					 sizeof(route_ctx->response_body),
					 "{\"authenticated\":false}");
	}

	ret = panel_status_render_json(route_ctx->app_context,
			       route_ctx->response_body,
			       sizeof(route_ctx->response_body));
	if (ret < 0) {
		return panel_http_write_json_response(response_ctx,
					 HTTP_500_INTERNAL_SERVER_ERROR,
					 route_ctx->response_body,
					 sizeof(route_ctx->response_body),
					 "{\"error\":\"status-render-failed\"}");
	}

	response_ctx->status = HTTP_200_OK;
	response_ctx->body = (const uint8_t *)route_ctx->response_body;
	response_ctx->body_len = (size_t)ret;
	response_ctx->final_chunk = true;
	return 0;
}

int panel_http_server_init(struct panel_http_server *server,
			   struct app_context *app_context)
{
	int ret;

	if (server == NULL || app_context == NULL) {
		return -EINVAL;
	}

	if (server->started) {
		return 0;
	}

	server->app_context = app_context;
	server->port = app_context->config.panel.port != 0 ?
		app_context->config.panel.port : APP_PANEL_PORT;
	server->started = false;
	panel_http_service_port = server->port;
	panel_auth_login_route_ctx.auth_service = &app_context->panel_auth;
	panel_auth_logout_route_ctx.auth_service = &app_context->panel_auth;
	panel_auth_session_route_ctx.auth_service = &app_context->panel_auth;
	panel_status_route_ctx.app_context = app_context;
	panel_relay_route_ctx.app_context = app_context;
	panel_schedule_snapshot_route_ctx.app_context = app_context;
	panel_schedule_create_route_ctx.app_context = app_context;
	panel_schedule_update_route_ctx.app_context = app_context;
	panel_schedule_delete_route_ctx.app_context = app_context;
	panel_schedule_enabled_route_ctx.app_context = app_context;

	ret = http_server_start();
	if (ret != 0) {
		LOG_ERR("Failed to start HTTP service shell: %d", ret);
		return ret;
	}

	server->started = true;
	LOG_INF("Panel HTTP shell listening on port %u", server->port);

	return 0;
}
