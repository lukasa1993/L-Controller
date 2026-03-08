#pragma once

struct app_context;

struct panel_auth_service {
	struct app_context *app_context;
};

int panel_auth_service_init(struct panel_auth_service *service,
			    struct app_context *app_context);
