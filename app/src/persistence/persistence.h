#pragma once

#include <stdbool.h>

#include <zephyr/fs/nvs.h>

#include "persistence/persistence_types.h"

struct app_config;
struct flash_area;

struct persistence_store {
	const struct app_config *config;
	const struct flash_area *flash_area;
	struct nvs_fs nvs;
	bool mounted;
};

bool persistence_store_is_ready(const struct persistence_store *store);

int persistence_store_init(struct persistence_store *store,
			   const struct app_config *config);

int persistence_store_load_auth(
	struct persistence_store *store,
	struct persisted_auth *auth,
	struct persistence_section_status *status);

int persistence_store_load_actions(
	struct persistence_store *store,
	struct persisted_action_catalog *actions,
	struct persistence_section_status *status);

int persistence_store_load_relay(
	struct persistence_store *store,
	struct persisted_relay *relay,
	struct persistence_section_status *status);

int persistence_store_load_schedule(
	struct persistence_store *store,
	const struct persisted_action_catalog *actions,
	struct persisted_schedule_table *schedule_table,
	struct persistence_section_status *status);

int persistence_store_load(struct persistence_store *store,
			   struct persisted_config *config);

int persistence_store_save_auth(
	struct persistence_store *store,
	struct persisted_config *config,
	const struct persisted_auth_save_request *request);

int persistence_store_save_actions(
	struct persistence_store *store,
	struct persisted_config *config,
	const struct persisted_action_catalog_save_request *request);

int persistence_store_save_relay(
	struct persistence_store *store,
	struct persisted_config *config,
	const struct persisted_relay_save_request *request);

int persistence_store_save_schedule(
	struct persistence_store *store,
	struct persisted_config *config,
	const struct persisted_schedule_table_save_request *request);

int persistence_store_save_config(
	struct persistence_store *store,
	struct persisted_config *config,
	const struct persisted_config_save_request *request);

const char *persistence_section_text(enum persistence_section section);

const char *persistence_load_state_text(enum persistence_load_state state);

const char *persisted_relay_reboot_policy_text(
	enum persisted_relay_reboot_policy policy);
