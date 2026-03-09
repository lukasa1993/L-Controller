#include <errno.h>

#include <zephyr/logging/log.h>
#include <zephyr/net/http/server.h>
#include <zephyr/net/http/service.h>

#include "app/app_context.h"
#include "panel/panel_http.h"

LOG_MODULE_REGISTER(panel_http, CONFIG_LOG_DEFAULT_LEVEL);

HTTP_SERVER_REGISTER_HEADER_CAPTURE(panel_http_cookie_header, "cookie");

static uint16_t panel_http_service_port = APP_PANEL_PORT;

HTTP_SERVICE_DEFINE_EMPTY(panel_http_service, NULL, &panel_http_service_port,
			  CONFIG_HTTP_SERVER_MAX_CLIENTS, 4, NULL, NULL, NULL);

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
