#pragma once

struct app_context;

struct recovery_manager {
	struct app_context *app_context;
};

int recovery_manager_init(struct recovery_manager *manager, struct app_context *app_context);
