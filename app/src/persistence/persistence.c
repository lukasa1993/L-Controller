#include <errno.h>

#include <pm_config.h>

#include <zephyr/drivers/flash.h>
#include <zephyr/logging/log.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/util.h>

#include "app/app_config.h"
#include "persistence/persistence.h"

LOG_MODULE_REGISTER(persistence, CONFIG_LOG_DEFAULT_LEVEL);

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

	ret = flash_get_page_info_by_offs(flash_area->fa_dev,
					  flash_area->fa_off,
					  &page_info);
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

int persistence_store_load(struct persistence_store *store,
			   struct persisted_config *config)
{
	ARG_UNUSED(store);
	ARG_UNUSED(config);

	return -ENOTSUP;
}

int persistence_store_save_auth(struct persistence_store *store,
				const struct persisted_auth *auth)
{
	ARG_UNUSED(store);
	ARG_UNUSED(auth);

	return -ENOTSUP;
}

int persistence_store_save_actions(struct persistence_store *store,
			   const struct persisted_action_catalog *actions)
{
	ARG_UNUSED(store);
	ARG_UNUSED(actions);

	return -ENOTSUP;
}

int persistence_store_save_relay(struct persistence_store *store,
			 const struct persisted_relay *relay)
{
	ARG_UNUSED(store);
	ARG_UNUSED(relay);

	return -ENOTSUP;
}

int persistence_store_save_schedule(struct persistence_store *store,
			    const struct persisted_schedule_table *schedule_table)
{
	ARG_UNUSED(store);
	ARG_UNUSED(schedule_table);

	return -ENOTSUP;
}
