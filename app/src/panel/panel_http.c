#include <errno.h>

#include <zephyr/logging/log.h>
#include <zephyr/net/http/server.h>
#include <zephyr/net/http/service.h>

#include "app/app_context.h"
#include "panel/panel_http.h"

LOG_MODULE_REGISTER(panel_http, CONFIG_LOG_DEFAULT_LEVEL);

HTTP_SERVER_REGISTER_HEADER_CAPTURE(panel_http_cookie_header, "cookie");

static const uint8_t index_html_gz[] = {
#include "panel/index.html.gz.inc"
};

static const uint8_t main_js_gz[] = {
#include "panel/main.js.gz.inc"
};

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

static uint16_t panel_http_service_port = APP_PANEL_PORT;

HTTP_SERVICE_DEFINE(panel_http_service, NULL, &panel_http_service_port,
		    CONFIG_HTTP_SERVER_MAX_CLIENTS, 4, NULL, NULL, NULL);

HTTP_RESOURCE_DEFINE(panel_shell_index_resource, panel_http_service, "/",
		     &panel_shell_index_resource_detail);
HTTP_RESOURCE_DEFINE(panel_shell_main_js_resource, panel_http_service, "/main.js",
		     &panel_shell_main_js_resource_detail);

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

	ret = http_server_start();
	if (ret != 0) {
		LOG_ERR("Failed to start HTTP service shell: %d", ret);
		return ret;
	}

	server->started = true;
	LOG_INF("Panel HTTP shell listening on port %u", server->port);

	return 0;
}
