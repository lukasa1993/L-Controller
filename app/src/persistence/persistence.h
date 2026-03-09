#pragma once

#include <stdbool.h>

#include <zephyr/fs/nvs.h>

#include "persistence/persistence_types.h"

struct app_config;
struct flash_area;
struct persisted_action_catalog;
struct persisted_auth;
struct persisted_config;
struct persisted_relay;
struct persisted_schedule_table;

struct persistence_store {
	const struct app_config *config;
	const struct flash_area *flash_area;
	struct nvs_fs nvs;
	bool mounted;
};

int persistence_store_init(struct persistence_store *store,
			   const struct app_config *config);
int persistence_store_load(struct persistence_store *store,
			   struct persisted_config *config);
int persistence_store_save_auth(struct persistence_store *store,
				const struct persisted_auth *auth);
int persistence_store_save_actions(struct persistence_store *store,
			   const struct persisted_action_catalog *actions);
int persistence_store_save_relay(struct persistence_store *store,
			 const struct persisted_relay *relay);
int persistence_store_save_schedule(struct persistence_store *store,
			    const struct persisted_schedule_table *schedule_table);
