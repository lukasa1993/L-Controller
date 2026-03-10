#include <errno.h>
#include <string.h>

#include <pm_config.h>

#include <zephyr/dfu/mcuboot.h>
#include <zephyr/logging/log.h>

#include "app/app_context.h"
#include "ota/ota.h"

LOG_MODULE_REGISTER(ota, CONFIG_LOG_DEFAULT_LEVEL);

static void ota_runtime_status_apply_persisted(struct ota_runtime_status *status,
					       const struct persisted_ota *persisted)
{
	status->state = persisted->state;
	status->staged_version = persisted->staged_version;
	status->last_attempt = persisted->last_attempt;
	status->remote_policy = persisted->remote_policy;
}

static void ota_runtime_status_clear_version(struct persisted_ota_version *version)
{
	memset(version, 0, sizeof(*version));
}

static void ota_runtime_status_load_current_version(struct ota_runtime_status *status)
{
	struct mcuboot_img_header header;
	int ret;

	ota_runtime_status_clear_version(&status->current_version);

	ret = boot_read_bank_header(PM_MCUBOOT_PRIMARY_ID, &header, sizeof(header));
	if (ret != 0) {
		LOG_WRN("Failed to read current image header: %d", ret);
		return;
	}

	status->current_version = (struct persisted_ota_version){
		.available = true,
		.major = header.h.v1.sem_ver.major,
		.minor = header.h.v1.sem_ver.minor,
		.revision = header.h.v1.sem_ver.revision,
		.build_num = header.h.v1.sem_ver.build_num,
		.image_size = header.h.v1.image_size,
	};
}

int ota_service_init(struct ota_service *service, struct app_context *app_context)
{
	if (service == NULL || app_context == NULL) {
		return -EINVAL;
	}

	memset(service, 0, sizeof(*service));
	service->app_context = app_context;
	service->status.implemented = true;
	k_mutex_init(&service->lock);

	ota_runtime_status_apply_persisted(&service->status, &app_context->persisted_config.ota);
	ota_runtime_status_load_current_version(&service->status);
	service->status.image_confirmed = boot_is_img_confirmed();

	return 0;
}

const struct ota_runtime_status *ota_service_get_status(const struct ota_service *service)
{
	if (service == NULL) {
		return NULL;
	}

	return &service->status;
}

int ota_service_copy_snapshot(const struct ota_service *service,
			      struct ota_runtime_status *status_out)
{
	if (service == NULL || status_out == NULL) {
		return -EINVAL;
	}

	*status_out = service->status;
	return 0;
}
