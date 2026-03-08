#pragma once

struct app_context;

struct relay_service {
	struct app_context *app_context;
};

int relay_service_init(struct relay_service *service, struct app_context *app_context);
