#pragma once

struct app_context;

struct ota_service {
	struct app_context *app_context;
};

int ota_service_init(struct ota_service *service, struct app_context *app_context);
