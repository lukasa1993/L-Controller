#pragma once

struct app_context;

struct scheduler_service {
	struct app_context *app_context;
};

int scheduler_service_init(struct scheduler_service *service,
			   struct app_context *app_context);
