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
	char response_body[1536];
	struct http_header headers[1];
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

static void panel_login_route_reset(struct panel_login_route_context *route_ctx)
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

	ret = http_server_start();
	if (ret != 0) {
		LOG_ERR("Failed to start HTTP service shell: %d", ret);
		return ret;
	}

	server->started = true;
	LOG_INF("Panel HTTP shell listening on port %u", server->port);

	return 0;
}
