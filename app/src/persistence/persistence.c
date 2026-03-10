#include <errno.h>
#include <string.h>

#include <pm_config.h>

#include <zephyr/drivers/flash.h>
#include <zephyr/logging/log.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/util.h>

#include "app/app_config.h"
#include "persistence/persistence.h"
#include "scheduler/scheduler.h"

LOG_MODULE_REGISTER(persistence, CONFIG_LOG_DEFAULT_LEVEL);

enum persistence_nvs_id {
	PERSISTENCE_NVS_ID_AUTH = 0x4001,
	PERSISTENCE_NVS_ID_ACTIONS = 0x4002,
	PERSISTENCE_NVS_ID_RELAY = 0x4003,
	PERSISTENCE_NVS_ID_SCHEDULE = 0x4004,
};

struct persistence_migration_plan {
	enum persistence_section section;
	uint32_t stored_schema_version;
	uint32_t expected_schema_version;
	enum persistence_migration_action action;
};

static uint32_t persistence_expected_layout_version(const struct persistence_store *store)
{
	return store->config->persistence.layout_version;
}

static struct persistence_section_status persistence_make_status(
	enum persistence_section section,
	enum persistence_load_state state,
	bool reseeded,
	enum persistence_migration_action migration_action,
	uint32_t stored_schema_version,
	uint32_t expected_schema_version)
{
	return (struct persistence_section_status){
		.section = section,
		.state = state,
		.migration_action = migration_action,
		.stored_schema_version = stored_schema_version,
		.expected_schema_version = expected_schema_version,
		.reseeded = reseeded,
	};
}

static struct persistence_migration_plan persistence_plan_section_migration(
	const struct persistence_store *store,
	enum persistence_section section,
	uint32_t stored_schema_version)
{
	const uint32_t expected_schema_version = persistence_expected_layout_version(store);
	struct persistence_migration_plan plan = {
		.section = section,
		.stored_schema_version = stored_schema_version,
		.expected_schema_version = expected_schema_version,
		.action = PERSISTENCE_MIGRATION_ACTION_NONE,
	};

	if (stored_schema_version == expected_schema_version) {
		return plan;
	}

	plan.action = section == PERSISTENCE_SECTION_AUTH ?
		PERSISTENCE_MIGRATION_ACTION_RESEED_FROM_CONFIG :
		PERSISTENCE_MIGRATION_ACTION_RESET_TO_DEFAULTS;

	return plan;
}

bool persistence_store_is_ready(const struct persistence_store *store)
{
	return store != NULL && store->config != NULL && store->mounted;
}

const char *persistence_section_text(enum persistence_section section)
{
	switch (section) {
	case PERSISTENCE_SECTION_AUTH:
		return "auth";
	case PERSISTENCE_SECTION_ACTIONS:
		return "actions";
	case PERSISTENCE_SECTION_RELAY:
		return "relay";
	case PERSISTENCE_SECTION_SCHEDULE:
		return "schedule";
	default:
		return "unknown";
	}
}

const char *persistence_load_state_text(enum persistence_load_state state)
{
	switch (state) {
	case PERSISTENCE_LOAD_STATE_LOADED:
		return "loaded";
	case PERSISTENCE_LOAD_STATE_EMPTY_DEFAULT:
		return "empty-default";
	case PERSISTENCE_LOAD_STATE_INVALID_RESET:
		return "corrupt-reset";
	case PERSISTENCE_LOAD_STATE_INCOMPATIBLE_RESET:
		return "incompatible-reset";
	default:
		return "unknown";
	}
}

const char *persistence_migration_action_text(
	enum persistence_migration_action action)
{
	switch (action) {
	case PERSISTENCE_MIGRATION_ACTION_NONE:
		return "none";
	case PERSISTENCE_MIGRATION_ACTION_RESET_TO_DEFAULTS:
		return "reset-to-defaults";
	case PERSISTENCE_MIGRATION_ACTION_RESEED_FROM_CONFIG:
		return "reseed-from-config";
	default:
		return "unknown";
	}
}

const char *persisted_relay_reboot_policy_text(enum persisted_relay_reboot_policy policy)
{
	switch (policy) {
	case PERSISTED_RELAY_REBOOT_POLICY_SAFE_OFF:
		return "safe-off";
	case PERSISTED_RELAY_REBOOT_POLICY_RESTORE_LAST_DESIRED:
		return "restore-last-desired";
	case PERSISTED_RELAY_REBOOT_POLICY_IGNORE_LAST_DESIRED:
		return "ignore-last-desired";
	default:
		return "unknown";
	}
}

static bool persisted_relay_reboot_policy_valid(enum persisted_relay_reboot_policy policy)
{
	switch (policy) {
	case PERSISTED_RELAY_REBOOT_POLICY_SAFE_OFF:
	case PERSISTED_RELAY_REBOOT_POLICY_RESTORE_LAST_DESIRED:
	case PERSISTED_RELAY_REBOOT_POLICY_IGNORE_LAST_DESIRED:
		return true;
	default:
		return false;
	}
}

static bool c_string_has_terminator(const char *value, size_t value_len)
{
	return value != NULL && memchr(value, '\0', value_len) != NULL;
}

static bool c_string_is_non_empty(const char *value, size_t value_len)
{
	return value != NULL && value[0] != '\0' && c_string_has_terminator(value, value_len);
}

static int copy_c_string(char *dst, size_t dst_len, const char *src)
{
	size_t src_len;

	if (dst == NULL || src == NULL || dst_len == 0U) {
		return -EINVAL;
	}

	src_len = strlen(src);
	if (src_len >= dst_len) {
		return -ENAMETOOLONG;
	}

	memcpy(dst, src, src_len + 1U);
	return 0;
}

static int persisted_auth_defaults(
	const struct persistence_store *store,
	struct persisted_auth *auth)
{
	int ret;

	memset(auth, 0, sizeof(*auth));
	auth->schema_version = persistence_expected_layout_version(store);

	ret = copy_c_string(auth->username, sizeof(auth->username), CONFIG_APP_ADMIN_USERNAME);
	if (ret != 0) {
		return ret;
	}

	return copy_c_string(auth->password, sizeof(auth->password), CONFIG_APP_ADMIN_PASSWORD);
}

static void persisted_action_defaults(
	const struct persistence_store *store,
	struct persisted_action_catalog *actions)
{
	memset(actions, 0, sizeof(*actions));
	actions->schema_version = persistence_expected_layout_version(store);
}

static void persisted_relay_defaults(
	const struct persistence_store *store,
	struct persisted_relay *relay)
{
	enum persisted_relay_reboot_policy default_policy =
		store->config->persistence.default_relay_reboot_policy;

	if (!persisted_relay_reboot_policy_valid(default_policy)) {
		default_policy = PERSISTED_RELAY_REBOOT_POLICY_SAFE_OFF;
	}

	memset(relay, 0, sizeof(*relay));
	relay->schema_version = persistence_expected_layout_version(store);
	relay->reboot_policy = default_policy;
}

static void persisted_schedule_defaults(
	const struct persistence_store *store,
	struct persisted_schedule_table *schedule_table)
{
	memset(schedule_table, 0, sizeof(*schedule_table));
	schedule_table->schema_version = persistence_expected_layout_version(store);
}

static enum persistence_load_state persistence_read_blob(
	struct persistence_store *store,
	uint16_t id,
	void *data,
	size_t data_len)
{
	ssize_t ret;

	ret = nvs_read(&store->nvs, id, data, data_len);
	if (ret == -ENOENT || ret == 0) {
		return PERSISTENCE_LOAD_STATE_EMPTY_DEFAULT;
	}

	if (ret < 0 || (size_t)ret != data_len) {
		return PERSISTENCE_LOAD_STATE_INVALID_RESET;
	}

	return PERSISTENCE_LOAD_STATE_LOADED;
}

static int persistence_write_blob(
	struct persistence_store *store,
	uint16_t id,
	const void *data,
	size_t data_len)
{
	ssize_t ret;

	ret = nvs_write(&store->nvs, id, data, data_len);
	if (ret < 0) {
		return (int)ret;
	}

	if (ret == 0 || (size_t)ret == data_len) {
		return 0;
	}

	return -EIO;
}

static bool persisted_auth_valid(const struct persisted_auth *auth)
{
	return auth != NULL &&
	       c_string_is_non_empty(auth->username, sizeof(auth->username)) &&
	       c_string_is_non_empty(auth->password, sizeof(auth->password));
}

static bool persisted_action_catalog_valid(const struct persisted_action_catalog *actions)
{
	uint32_t index;
	uint32_t match_index;

	if (actions == NULL || actions->count > PERSISTED_ACTION_MAX_COUNT) {
		return false;
	}

	for (index = 0U; index < actions->count; ++index) {
		const struct persisted_action *action = &actions->entries[index];

		if (!c_string_is_non_empty(action->action_id, sizeof(action->action_id))) {
			return false;
		}

		for (match_index = index + 1U; match_index < actions->count; ++match_index) {
			if (strcmp(action->action_id,
				   actions->entries[match_index].action_id) == 0) {
				return false;
			}
		}
	}

	return true;
}

static bool persisted_relay_valid(const struct persisted_relay *relay)
{
	return relay != NULL &&
	       persisted_relay_reboot_policy_valid(relay->reboot_policy);
}

static int persisted_schedule_table_validate(
	const struct persisted_schedule_table *schedule_table,
	const struct persisted_action_catalog *actions)
{
	return scheduler_schedule_table_validate(schedule_table, actions);
}

static bool persisted_config_save_request_has_changes(
	const struct persisted_config_save_request *request)
{
	return request != NULL &&
	       (request->has_auth || request->has_actions || request->has_relay ||
		request->has_schedule);
}

static void persisted_auth_from_save_request(
	const struct persistence_store *store,
	const struct persisted_auth_save_request *request,
	struct persisted_auth *auth)
{
	memset(auth, 0, sizeof(*auth));
	auth->schema_version = persistence_expected_layout_version(store);
	memcpy(auth->username, request->username, sizeof(auth->username));
	memcpy(auth->password, request->password, sizeof(auth->password));
}

static void persisted_action_catalog_from_save_request(
	const struct persistence_store *store,
	const struct persisted_action_catalog_save_request *request,
	struct persisted_action_catalog *actions)
{
	uint32_t copy_count = MIN(request->count, PERSISTED_ACTION_MAX_COUNT);

	memset(actions, 0, sizeof(*actions));
	actions->schema_version = persistence_expected_layout_version(store);
	actions->count = request->count;
	memcpy(actions->entries, request->entries,
	       sizeof(actions->entries[0]) * copy_count);
}

static void persisted_relay_from_save_request(
	const struct persistence_store *store,
	const struct persisted_relay_save_request *request,
	struct persisted_relay *relay)
{
	memset(relay, 0, sizeof(*relay));
	relay->schema_version = persistence_expected_layout_version(store);
	relay->last_desired_state = request->last_desired_state;
	relay->reboot_policy = request->reboot_policy;
}

static void persisted_schedule_table_from_save_request(
	const struct persistence_store *store,
	const struct persisted_schedule_table_save_request *request,
	struct persisted_schedule_table *schedule_table)
{
	uint32_t copy_count = MIN(request->count, PERSISTED_SCHEDULE_MAX_COUNT);

	memset(schedule_table, 0, sizeof(*schedule_table));
	schedule_table->schema_version = persistence_expected_layout_version(store);
	schedule_table->count = request->count;
	memcpy(schedule_table->entries, request->entries,
	       sizeof(schedule_table->entries[0]) * copy_count);
}

static void persisted_config_mark_section_loaded(
	struct persisted_config *config,
	enum persistence_section section)
{
	struct persistence_section_status status =
		persistence_make_status(section,
					PERSISTENCE_LOAD_STATE_LOADED,
					false,
					PERSISTENCE_MIGRATION_ACTION_NONE,
					config->layout_version,
					config->layout_version);

	switch (section) {
	case PERSISTENCE_SECTION_AUTH:
		config->load_report.auth = status;
		break;
	case PERSISTENCE_SECTION_ACTIONS:
		config->load_report.actions = status;
		break;
	case PERSISTENCE_SECTION_RELAY:
		config->load_report.relay = status;
		break;
	case PERSISTENCE_SECTION_SCHEDULE:
		config->load_report.schedule = status;
		break;
	default:
		break;
	}
}

static int persistence_stage_config_save_request(
	struct persistence_store *store,
	const struct persisted_config *current,
	const struct persisted_config_save_request *request,
	struct persisted_config *staged)
{
	if (!persistence_store_is_ready(store) || current == NULL || request == NULL ||
	    staged == NULL || !persisted_config_save_request_has_changes(request)) {
		return -EINVAL;
	}

	*staged = *current;
	staged->layout_version = persistence_expected_layout_version(store);

	if (request->has_auth) {
		persisted_auth_from_save_request(store, &request->auth, &staged->auth);
	}

	if (request->has_actions) {
		persisted_action_catalog_from_save_request(store, &request->actions,
							 &staged->actions);
	}

	if (request->has_relay) {
		persisted_relay_from_save_request(store, &request->relay, &staged->relay);
	}

	if (request->has_schedule) {
		persisted_schedule_table_from_save_request(store, &request->schedule,
						  &staged->schedule);
	}

	if (!persisted_auth_valid(&staged->auth) ||
	    !persisted_action_catalog_valid(&staged->actions) ||
	    !persisted_relay_valid(&staged->relay)) {
		return -EINVAL;
	}

	return persisted_schedule_table_validate(&staged->schedule,
					       &staged->actions);
}

int persistence_store_init(struct persistence_store *store,
			   const struct app_config *config)
{
	const struct flash_area *flash_area;
	struct flash_pages_info page_info;
	int ret;

	if (store == NULL || config == NULL) {
		return -EINVAL;
	}

	ret = flash_area_open(PM_SETTINGS_STORAGE_ID, &flash_area);
	if (ret != 0) {
		LOG_ERR("Failed to open settings storage partition: %d", ret);
		return ret;
	}

	if (!flash_area_device_is_ready(flash_area)) {
		flash_area_close(flash_area);
		return -ENODEV;
	}

	ret = flash_get_page_info_by_offs(flash_area->fa_dev, flash_area->fa_off, &page_info);
	if (ret != 0) {
		LOG_ERR("Failed to query storage page info: %d", ret);
		flash_area_close(flash_area);
		return ret;
	}

	if (page_info.size == 0U || (flash_area->fa_size % page_info.size) != 0U) {
		LOG_ERR("Storage partition size %zu is not aligned to page size %zu",
			flash_area->fa_size, page_info.size);
		flash_area_close(flash_area);
		return -EINVAL;
	}

	*store = (struct persistence_store){
		.config = config,
		.flash_area = flash_area,
		.nvs = {
			.offset = flash_area->fa_off,
			.sector_size = page_info.size,
			.sector_count = flash_area->fa_size / page_info.size,
			.flash_device = flash_area->fa_dev,
		},
	};

	if (store->nvs.sector_count < 2U) {
		LOG_ERR("Settings partition must expose at least two sectors, got %u",
			store->nvs.sector_count);
		flash_area_close(store->flash_area);
		store->flash_area = NULL;
		return -EINVAL;
	}

	ret = nvs_mount(&store->nvs);
	if (ret != 0) {
		LOG_ERR("Failed to mount persistence NVS store: %d", ret);
		flash_area_close(store->flash_area);
		store->flash_area = NULL;
		return ret;
	}

	store->mounted = true;

	LOG_INF("Mounted persistence store at 0x%lx (%u sectors x %lu bytes)",
		(long)store->nvs.offset,
		store->nvs.sector_count,
		(unsigned long)store->nvs.sector_size);

	return 0;
}

static int persistence_write_auth_section(struct persistence_store *store,
					 const struct persisted_auth *auth)
{
	struct persisted_auth canonical;

	if (!persistence_store_is_ready(store) || auth == NULL) {
		return -EINVAL;
	}

	canonical = *auth;
	canonical.schema_version = persistence_expected_layout_version(store);
	if (!persisted_auth_valid(&canonical)) {
		return -EINVAL;
	}

	return persistence_write_blob(store, PERSISTENCE_NVS_ID_AUTH, &canonical,
				      sizeof(canonical));
}

static int persistence_write_actions_section(
	struct persistence_store *store,
	const struct persisted_action_catalog *actions)
{
	struct persisted_action_catalog canonical;

	if (!persistence_store_is_ready(store) || actions == NULL) {
		return -EINVAL;
	}

	canonical = *actions;
	canonical.schema_version = persistence_expected_layout_version(store);
	if (!persisted_action_catalog_valid(&canonical)) {
		return -EINVAL;
	}

	return persistence_write_blob(store, PERSISTENCE_NVS_ID_ACTIONS, &canonical,
				      sizeof(canonical));
}

static int persistence_write_relay_section(struct persistence_store *store,
					   const struct persisted_relay *relay)
{
	struct persisted_relay canonical;

	if (!persistence_store_is_ready(store) || relay == NULL) {
		return -EINVAL;
	}

	canonical = *relay;
	canonical.schema_version = persistence_expected_layout_version(store);
	if (!persisted_relay_valid(&canonical)) {
		return -EINVAL;
	}

	return persistence_write_blob(store, PERSISTENCE_NVS_ID_RELAY, &canonical,
				      sizeof(canonical));
}

static int persistence_write_schedule_section(
	struct persistence_store *store,
	const struct persisted_action_catalog *actions,
	const struct persisted_schedule_table *schedule_table)
{
	struct persisted_schedule_table canonical;
	int ret;

	if (!persistence_store_is_ready(store) || schedule_table == NULL) {
		return -EINVAL;
	}

	canonical = *schedule_table;
	canonical.schema_version = persistence_expected_layout_version(store);
	ret = persisted_schedule_table_validate(&canonical, actions);
	if (ret != 0) {
		return ret;
	}

	return persistence_write_blob(store, PERSISTENCE_NVS_ID_SCHEDULE, &canonical,
				      sizeof(canonical));
}

int persistence_store_save_config(
	struct persistence_store *store,
	struct persisted_config *config,
	const struct persisted_config_save_request *request)
{
	struct persisted_config staged;
	int ret;
	int reload_ret;

	if (!persistence_store_is_ready(store) || config == NULL || request == NULL) {
		return -EINVAL;
	}

	ret = persistence_stage_config_save_request(store, config, request, &staged);
	if (ret != 0) {
		return ret;
	}

	if (request->has_auth) {
		ret = persistence_write_auth_section(store, &staged.auth);
		if (ret != 0) {
			goto reload_snapshot;
		}
	}

	if (request->has_actions) {
		ret = persistence_write_actions_section(store, &staged.actions);
		if (ret != 0) {
			goto reload_snapshot;
		}
	}

	if (request->has_relay) {
		ret = persistence_write_relay_section(store, &staged.relay);
		if (ret != 0) {
			goto reload_snapshot;
		}
	}

	if (request->has_schedule) {
		ret = persistence_write_schedule_section(store, &staged.actions,
							  &staged.schedule);
		if (ret != 0) {
			goto reload_snapshot;
		}
	}

	*config = staged;
	if (request->has_auth) {
		persisted_config_mark_section_loaded(config, PERSISTENCE_SECTION_AUTH);
	}

	if (request->has_actions) {
		persisted_config_mark_section_loaded(config, PERSISTENCE_SECTION_ACTIONS);
	}

	if (request->has_relay) {
		persisted_config_mark_section_loaded(config, PERSISTENCE_SECTION_RELAY);
	}

	if (request->has_schedule) {
		persisted_config_mark_section_loaded(config, PERSISTENCE_SECTION_SCHEDULE);
	}

	return 0;

reload_snapshot:
	reload_ret = persistence_store_load(store, config);
	if (reload_ret != 0) {
		LOG_ERR("Failed to reload persistence snapshot after save error: %d",
			reload_ret);
	}

	return ret;
}

int persistence_store_save_auth(
	struct persistence_store *store,
	struct persisted_config *config,
	const struct persisted_auth_save_request *request)
{
	struct persisted_config_save_request save_request;

	if (request == NULL) {
		return -EINVAL;
	}

	save_request = (struct persisted_config_save_request){
		.has_auth = true,
		.auth = *request,
	};

	return persistence_store_save_config(store, config, &save_request);
}

int persistence_store_save_actions(
	struct persistence_store *store,
	struct persisted_config *config,
	const struct persisted_action_catalog_save_request *request)
{
	struct persisted_config_save_request save_request;

	if (request == NULL) {
		return -EINVAL;
	}

	save_request = (struct persisted_config_save_request){
		.has_actions = true,
		.actions = *request,
	};

	return persistence_store_save_config(store, config, &save_request);
}

int persistence_store_save_relay(
	struct persistence_store *store,
	struct persisted_config *config,
	const struct persisted_relay_save_request *request)
{
	struct persisted_config_save_request save_request;

	if (request == NULL) {
		return -EINVAL;
	}

	save_request = (struct persisted_config_save_request){
		.has_relay = true,
		.relay = *request,
	};

	return persistence_store_save_config(store, config, &save_request);
}

int persistence_store_save_schedule(
	struct persistence_store *store,
	struct persisted_config *config,
	const struct persisted_schedule_table_save_request *request)
{
	struct persisted_config_save_request save_request;

	if (request == NULL) {
		return -EINVAL;
	}

	save_request = (struct persisted_config_save_request){
		.has_schedule = true,
		.schedule = *request,
	};

	return persistence_store_save_config(store, config, &save_request);
}

int persistence_store_load_auth(
	struct persistence_store *store,
	struct persisted_auth *auth,
	struct persistence_section_status *status)
{
	struct persisted_auth candidate;
	struct persistence_migration_plan migration_plan;
	enum persistence_load_state state;
	int ret;

	if (!persistence_store_is_ready(store) || auth == NULL || status == NULL) {
		return -EINVAL;
	}

	memset(&candidate, 0, sizeof(candidate));
	migration_plan = persistence_plan_section_migration(store,
						  PERSISTENCE_SECTION_AUTH,
						  0U);
	state = persistence_read_blob(store, PERSISTENCE_NVS_ID_AUTH, &candidate,
				      sizeof(candidate));
	if (state == PERSISTENCE_LOAD_STATE_LOADED) {
		migration_plan = persistence_plan_section_migration(store,
							  PERSISTENCE_SECTION_AUTH,
							  candidate.schema_version);
		if (migration_plan.action != PERSISTENCE_MIGRATION_ACTION_NONE) {
			state = PERSISTENCE_LOAD_STATE_INCOMPATIBLE_RESET;
		} else if (persisted_auth_valid(&candidate)) {
			*auth = candidate;
			*status = persistence_make_status(PERSISTENCE_SECTION_AUTH,
						      state,
						      false,
						      migration_plan.action,
						      candidate.schema_version,
						      migration_plan.expected_schema_version);
			return 0;
		} else {
			state = PERSISTENCE_LOAD_STATE_INVALID_RESET;
		}
	}

	ret = persisted_auth_defaults(store, auth);
	if (ret != 0) {
		return ret;
	}

	*status = persistence_make_status(PERSISTENCE_SECTION_AUTH,
					  state,
					  true,
					  state == PERSISTENCE_LOAD_STATE_INCOMPATIBLE_RESET ?
						  migration_plan.action :
						  PERSISTENCE_MIGRATION_ACTION_NONE,
					  candidate.schema_version,
					  migration_plan.expected_schema_version);
	return persistence_write_auth_section(store, auth);
}

int persistence_store_load_actions(
	struct persistence_store *store,
	struct persisted_action_catalog *actions,
	struct persistence_section_status *status)
{
	struct persisted_action_catalog candidate;
	struct persistence_migration_plan migration_plan;
	enum persistence_load_state state;
	int ret;

	if (!persistence_store_is_ready(store) || actions == NULL || status == NULL) {
		return -EINVAL;
	}

	memset(&candidate, 0, sizeof(candidate));
	migration_plan = persistence_plan_section_migration(store,
						  PERSISTENCE_SECTION_ACTIONS,
						  0U);
	state = persistence_read_blob(store, PERSISTENCE_NVS_ID_ACTIONS, &candidate,
				      sizeof(candidate));
	if (state == PERSISTENCE_LOAD_STATE_LOADED) {
		migration_plan = persistence_plan_section_migration(store,
							  PERSISTENCE_SECTION_ACTIONS,
							  candidate.schema_version);
		if (migration_plan.action != PERSISTENCE_MIGRATION_ACTION_NONE) {
			state = PERSISTENCE_LOAD_STATE_INCOMPATIBLE_RESET;
		} else if (persisted_action_catalog_valid(&candidate)) {
			*actions = candidate;
			*status = persistence_make_status(PERSISTENCE_SECTION_ACTIONS,
						      state,
						      false,
						      migration_plan.action,
						      candidate.schema_version,
						      migration_plan.expected_schema_version);
			return 0;
		} else {
			state = PERSISTENCE_LOAD_STATE_INVALID_RESET;
		}
	}

	persisted_action_defaults(store, actions);
	*status = persistence_make_status(PERSISTENCE_SECTION_ACTIONS,
					  state,
					  false,
					  state == PERSISTENCE_LOAD_STATE_INCOMPATIBLE_RESET ?
						  migration_plan.action :
						  PERSISTENCE_MIGRATION_ACTION_NONE,
					  candidate.schema_version,
					  migration_plan.expected_schema_version);
	if (state == PERSISTENCE_LOAD_STATE_EMPTY_DEFAULT) {
		return 0;
	}

	ret = persistence_write_actions_section(store, actions);
	return ret;
}

int persistence_store_load_relay(
	struct persistence_store *store,
	struct persisted_relay *relay,
	struct persistence_section_status *status)
{
	struct persisted_relay candidate;
	struct persistence_migration_plan migration_plan;
	enum persistence_load_state state;
	int ret;

	if (!persistence_store_is_ready(store) || relay == NULL || status == NULL) {
		return -EINVAL;
	}

	memset(&candidate, 0, sizeof(candidate));
	migration_plan = persistence_plan_section_migration(store,
						  PERSISTENCE_SECTION_RELAY,
						  0U);
	state = persistence_read_blob(store, PERSISTENCE_NVS_ID_RELAY, &candidate,
				      sizeof(candidate));
	if (state == PERSISTENCE_LOAD_STATE_LOADED) {
		migration_plan = persistence_plan_section_migration(store,
							  PERSISTENCE_SECTION_RELAY,
							  candidate.schema_version);
		if (migration_plan.action != PERSISTENCE_MIGRATION_ACTION_NONE) {
			state = PERSISTENCE_LOAD_STATE_INCOMPATIBLE_RESET;
		} else if (persisted_relay_valid(&candidate)) {
			*relay = candidate;
			*status = persistence_make_status(PERSISTENCE_SECTION_RELAY,
						      state,
						      false,
						      migration_plan.action,
						      candidate.schema_version,
						      migration_plan.expected_schema_version);
			return 0;
		} else {
			state = PERSISTENCE_LOAD_STATE_INVALID_RESET;
		}
	}

	persisted_relay_defaults(store, relay);
	*status = persistence_make_status(PERSISTENCE_SECTION_RELAY,
					  state,
					  false,
					  state == PERSISTENCE_LOAD_STATE_INCOMPATIBLE_RESET ?
						  migration_plan.action :
						  PERSISTENCE_MIGRATION_ACTION_NONE,
					  candidate.schema_version,
					  migration_plan.expected_schema_version);
	if (state == PERSISTENCE_LOAD_STATE_EMPTY_DEFAULT) {
		return 0;
	}

	ret = persistence_write_relay_section(store, relay);
	return ret;
}

int persistence_store_load_schedule(
	struct persistence_store *store,
	const struct persisted_action_catalog *actions,
	struct persisted_schedule_table *schedule_table,
	struct persistence_section_status *status)
{
	struct persisted_schedule_table candidate;
	struct persistence_migration_plan migration_plan;
	enum persistence_load_state state;
	int validate_ret;
	int ret;

	if (!persistence_store_is_ready(store) || schedule_table == NULL || status == NULL ||
	    actions == NULL) {
		return -EINVAL;
	}

	memset(&candidate, 0, sizeof(candidate));
	migration_plan = persistence_plan_section_migration(store,
						  PERSISTENCE_SECTION_SCHEDULE,
						  0U);
	state = persistence_read_blob(store, PERSISTENCE_NVS_ID_SCHEDULE, &candidate,
				      sizeof(candidate));
	if (state == PERSISTENCE_LOAD_STATE_LOADED) {
		migration_plan = persistence_plan_section_migration(store,
						  PERSISTENCE_SECTION_SCHEDULE,
						  candidate.schema_version);
		if (migration_plan.action != PERSISTENCE_MIGRATION_ACTION_NONE) {
			state = PERSISTENCE_LOAD_STATE_INCOMPATIBLE_RESET;
		} else {
			validate_ret = persisted_schedule_table_validate(&candidate, actions);
			if (validate_ret == 0) {
			*schedule_table = candidate;
			*status = persistence_make_status(PERSISTENCE_SECTION_SCHEDULE,
						      state,
						      false,
						      migration_plan.action,
						      candidate.schema_version,
						      migration_plan.expected_schema_version);
			return 0;
			}

			state = PERSISTENCE_LOAD_STATE_INVALID_RESET;
		}
	}

	persisted_schedule_defaults(store, schedule_table);
	*status = persistence_make_status(PERSISTENCE_SECTION_SCHEDULE,
					  state,
					  false,
					  state == PERSISTENCE_LOAD_STATE_INCOMPATIBLE_RESET ?
						  migration_plan.action :
						  PERSISTENCE_MIGRATION_ACTION_NONE,
					  candidate.schema_version,
					  migration_plan.expected_schema_version);
	if (state == PERSISTENCE_LOAD_STATE_EMPTY_DEFAULT) {
		return 0;
	}

	ret = persistence_write_schedule_section(store, actions, schedule_table);
	return ret;
}

int persistence_store_load(struct persistence_store *store,
			   struct persisted_config *config)
{
	int ret;

	if (!persistence_store_is_ready(store) || config == NULL) {
		return -EINVAL;
	}

	memset(config, 0, sizeof(*config));
	config->layout_version = persistence_expected_layout_version(store);

	ret = persistence_store_load_auth(store, &config->auth, &config->load_report.auth);
	if (ret != 0) {
		return ret;
	}

	ret = persistence_store_load_actions(store, &config->actions,
				     &config->load_report.actions);
	if (ret != 0) {
		return ret;
	}

	ret = persistence_store_load_relay(store, &config->relay, &config->load_report.relay);
	if (ret != 0) {
		return ret;
	}

	return persistence_store_load_schedule(store, &config->actions, &config->schedule,
				      &config->load_report.schedule);
}
