#pragma once

struct app_context;

struct panel_http_server {
	struct app_context *app_context;
};

int panel_http_server_init(struct panel_http_server *server,
			   struct app_context *app_context);
