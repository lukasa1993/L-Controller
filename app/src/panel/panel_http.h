#pragma once

#include <stdbool.h>
#include <stdint.h>

struct app_context;

struct panel_http_server {
	struct app_context *app_context;
	uint16_t port;
	bool started;
};

int panel_http_server_init(struct panel_http_server *server,
			   struct app_context *app_context);
