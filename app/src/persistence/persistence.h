#pragma once

struct app_context;

struct persistence_store {
	struct app_context *app_context;
};

int persistence_store_init(struct persistence_store *store,
			   struct app_context *app_context);
