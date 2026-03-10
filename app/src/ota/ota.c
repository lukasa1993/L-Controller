#include <errno.h>
#include <string.h>

#include <pm_config.h>

#include <zephyr/dfu/mcuboot.h>
#include <zephyr/logging/log.h>
#include <zephyr/storage/flash_map.h>

#include "app/app_context.h"
#include "ota/ota.h"
#include "persistence/persistence.h"

LOG_MODULE_REGISTER(ota, CONFIG_LOG_DEFAULT_LEVEL);

static void ota_version_clear(struct persisted_ota_version *version)
{
	memset(version, 0, sizeof(*version));
}

static void ota_attempt_clear(struct persisted_ota_attempt *attempt)
{
	memset(attempt, 0, sizeof(*attempt));
	attempt->result = PERSISTED_OTA_LAST_RESULT_NONE;
}

static void ota_runtime_status_apply_persisted(struct ota_runtime_status *status,
					       const struct persisted_ota *persisted)
{
	status->state = persisted->state;
	status->staged_version = persisted->staged_version;
	status->last_attempt = persisted->last_attempt;
	status->remote_policy = persisted->remote_policy;
}

static int ota_service_read_slot_version(uint8_t area_id,
					 struct persisted_ota_version *version_out)
{
	struct mcuboot_img_header header;
	int ret;

	if (version_out == NULL) {
		return -EINVAL;
	}

	ota_version_clear(version_out);

	ret = boot_read_bank_header(area_id, &header, sizeof(header));
	if (ret != 0) {
		return ret;
	}

	*version_out = (struct persisted_ota_version){
		.available = true,
		.major = header.h.v1.sem_ver.major,
		.minor = header.h.v1.sem_ver.minor,
		.revision = header.h.v1.sem_ver.revision,
		.build_num = header.h.v1.sem_ver.build_num,
		.image_size = header.h.v1.image_size,
	};

	return 0;
}

static void ota_runtime_status_load_current_version(struct ota_runtime_status *status)
{
	int ret;

	ret = ota_service_read_slot_version(PM_MCUBOOT_PRIMARY_ID, &status->current_version);
	if (ret != 0) {
		LOG_WRN("Failed to read current image header: %d", ret);
	}
}

static int ota_version_compare(const struct persisted_ota_version *candidate,
			       const struct persisted_ota_version *current)
{
	if (candidate == NULL || current == NULL || !candidate->available ||
	    !current->available) {
		return -EINVAL;
	}

	if (candidate->major != current->major) {
		return candidate->major > current->major ? 1 : -1;
	}

	if (candidate->minor != current->minor) {
		return candidate->minor > current->minor ? 1 : -1;
	}

	if (candidate->revision != current->revision) {
		return candidate->revision > current->revision ? 1 : -1;
	}

	if (candidate->build_num != current->build_num) {
		return candidate->build_num > current->build_num ? 1 : -1;
	}

	return 0;
}

static struct persisted_ota_attempt ota_make_attempt(
	enum persisted_ota_last_result_code result,
	int error_code,
	uint32_t bytes_written,
	const struct persisted_ota_version *version)
{
	struct persisted_ota_attempt attempt;

	ota_attempt_clear(&attempt);
	attempt.recorded = result != PERSISTED_OTA_LAST_RESULT_NONE;
	attempt.result = result;
	attempt.error_code = error_code;
	attempt.bytes_written = bytes_written;
	if (version != NULL) {
		attempt.version = *version;
	}

	return attempt;
}

static void ota_service_close_stage_context(struct ota_service *service)
{
	if (service->stage_context.flash_area != NULL) {
		flash_area_close(service->stage_context.flash_area);
		service->stage_context.flash_area = NULL;
	}

	service->stage_open = false;
}

static void ota_service_sync_status_locked(struct ota_service *service)
{
	struct persisted_ota_version slot_version;

	service->status = (struct ota_runtime_status){
		.implemented = true,
	};

	ota_runtime_status_apply_persisted(&service->status,
					   &service->app_context->persisted_config.ota);
	ota_runtime_status_load_current_version(&service->status);
	service->status.image_confirmed = boot_is_img_confirmed();

	if (service->status.state != PERSISTED_OTA_STATE_IDLE &&
	    ota_service_read_slot_version(PM_MCUBOOT_SECONDARY_ID, &slot_version) == 0) {
		service->status.staged_version = slot_version;
	}
}

static int ota_service_save_persisted_locked(struct ota_service *service,
					     const struct persisted_ota *ota)
{
	struct persisted_ota_save_request request = {
		.state = ota->state,
		.staged_version = ota->staged_version,
		.last_attempt = ota->last_attempt,
		.remote_policy = ota->remote_policy,
	};
	int ret;

	ret = persistence_store_save_ota(&service->app_context->persistence,
					 &service->app_context->persisted_config,
					 &request);
	if (ret != 0) {
		return ret;
	}

	ota_service_sync_status_locked(service);
	return 0;
}

static int ota_service_update_state_locked(struct ota_service *service,
					   enum persisted_ota_state state,
					   const struct persisted_ota_version *staged_version,
					   const struct persisted_ota_attempt *last_attempt)
{
	struct persisted_ota persisted = service->app_context->persisted_config.ota;

	persisted.state = state;
	if (staged_version != NULL) {
		persisted.staged_version = *staged_version;
	} else {
		ota_version_clear(&persisted.staged_version);
	}

	if (last_attempt != NULL) {
		persisted.last_attempt = *last_attempt;
	}

	return ota_service_save_persisted_locked(service, &persisted);
}

static int ota_service_record_stage_failure_locked(
	struct ota_service *service,
	int error_code,
	uint32_t bytes_written,
	const struct persisted_ota_version *version)
{
	struct persisted_ota_attempt attempt =
		ota_make_attempt(PERSISTED_OTA_LAST_RESULT_STAGE_FAILED,
				 error_code != 0 ? error_code : -ECANCELED,
				 bytes_written,
				 version);

	return ota_service_update_state_locked(service,
					      PERSISTED_OTA_STATE_IDLE,
					      NULL,
					      &attempt);
}

int ota_service_init(struct ota_service *service, struct app_context *app_context)
{
	if (service == NULL || app_context == NULL) {
		return -EINVAL;
	}

	memset(service, 0, sizeof(*service));
	service->app_context = app_context;
	k_mutex_init(&service->lock);
	ota_service_sync_status_locked(service);

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

int ota_service_begin_staging(struct ota_service *service)
{
	int ret;

	if (service == NULL) {
		return -EINVAL;
	}

	k_mutex_lock(&service->lock, K_FOREVER);
	if (service->stage_open) {
		k_mutex_unlock(&service->lock);
		return -EBUSY;
	}

	ret = flash_img_init_id(&service->stage_context, PM_MCUBOOT_SECONDARY_ID);
	if (ret != 0) {
		k_mutex_unlock(&service->lock);
		return ret;
	}

	service->stage_open = true;
	ret = ota_service_update_state_locked(service, PERSISTED_OTA_STATE_STAGING, NULL, NULL);
	if (ret != 0) {
		ota_service_close_stage_context(service);
		k_mutex_unlock(&service->lock);
		return ret;
	}

	k_mutex_unlock(&service->lock);
	return 0;
}

int ota_service_write_chunk(struct ota_service *service,
			    const uint8_t *data,
			    size_t data_len)
{
	int ret;

	if (service == NULL || (data == NULL && data_len > 0U)) {
		return -EINVAL;
	}

	k_mutex_lock(&service->lock, K_FOREVER);
	if (!service->stage_open) {
		k_mutex_unlock(&service->lock);
		return -EPIPE;
	}

	ret = flash_img_buffered_write(&service->stage_context, data, data_len, false);
	k_mutex_unlock(&service->lock);
	return ret;
}

int ota_service_abort_staging(struct ota_service *service, int error_code)
{
	uint32_t bytes_written;
	int ret;

	if (service == NULL) {
		return -EINVAL;
	}

	k_mutex_lock(&service->lock, K_FOREVER);
	if (!service->stage_open) {
		k_mutex_unlock(&service->lock);
		return -EALREADY;
	}

	bytes_written = flash_img_bytes_written(&service->stage_context);
	ota_service_close_stage_context(service);
	ret = ota_service_record_stage_failure_locked(service,
						      error_code,
						      bytes_written,
						      NULL);
	k_mutex_unlock(&service->lock);
	return ret;
}

int ota_service_finish_staging(struct ota_service *service)
{
	struct persisted_ota_version staged_version;
	struct persisted_ota_attempt attempt;
	uint32_t bytes_written;
	int compare_ret;
	int ret;
	int update_ret;

	if (service == NULL) {
		return -EINVAL;
	}

	k_mutex_lock(&service->lock, K_FOREVER);
	if (!service->stage_open) {
		k_mutex_unlock(&service->lock);
		return -EPIPE;
	}

	bytes_written = flash_img_bytes_written(&service->stage_context);
	ret = flash_img_buffered_write(&service->stage_context, NULL, 0U, true);
	ota_service_close_stage_context(service);
	if (ret != 0) {
		(void)ota_service_record_stage_failure_locked(service,
							      ret,
							      bytes_written,
							      NULL);
		k_mutex_unlock(&service->lock);
		return ret;
	}

	ret = ota_service_read_slot_version(PM_MCUBOOT_SECONDARY_ID, &staged_version);
	if (ret != 0) {
		attempt = ota_make_attempt(PERSISTED_OTA_LAST_RESULT_REJECTED_INVALID_IMAGE,
					 ret,
					 bytes_written,
					 NULL);
		(void)ota_service_update_state_locked(service, PERSISTED_OTA_STATE_IDLE,
						      NULL, &attempt);
		k_mutex_unlock(&service->lock);
		return ret;
	}

	if (!service->status.current_version.available) {
		attempt = ota_make_attempt(PERSISTED_OTA_LAST_RESULT_REJECTED_INVALID_IMAGE,
					 -ENODATA,
					 bytes_written,
					 &staged_version);
		(void)ota_service_update_state_locked(service, PERSISTED_OTA_STATE_IDLE,
						      NULL, &attempt);
		k_mutex_unlock(&service->lock);
		return -ENODATA;
	}

	compare_ret = ota_version_compare(&staged_version, &service->status.current_version);
	if (compare_ret == 0) {
		attempt = ota_make_attempt(PERSISTED_OTA_LAST_RESULT_REJECTED_SAME_VERSION,
					 -EALREADY,
					 bytes_written,
					 &staged_version);
		(void)ota_service_update_state_locked(service, PERSISTED_OTA_STATE_IDLE,
						      NULL, &attempt);
		k_mutex_unlock(&service->lock);
		return -EALREADY;
	}

	if (compare_ret < 0) {
		attempt = ota_make_attempt(PERSISTED_OTA_LAST_RESULT_REJECTED_DOWNGRADE,
					 -EPERM,
					 bytes_written,
					 &staged_version);
		(void)ota_service_update_state_locked(service, PERSISTED_OTA_STATE_IDLE,
						      NULL, &attempt);
		k_mutex_unlock(&service->lock);
		return -EPERM;
	}

	attempt = ota_make_attempt(PERSISTED_OTA_LAST_RESULT_STAGE_READY,
				 0,
				 bytes_written,
				 &staged_version);
	update_ret = ota_service_update_state_locked(service, PERSISTED_OTA_STATE_STAGED,
						     &staged_version,
						     &attempt);
	k_mutex_unlock(&service->lock);
	return update_ret;
}

int ota_service_clear_staged_image(struct ota_service *service)
{
	int ret;

	if (service == NULL) {
		return -EINVAL;
	}

	k_mutex_lock(&service->lock, K_FOREVER);
	if (service->status.state == PERSISTED_OTA_STATE_APPLY_REQUESTED) {
		k_mutex_unlock(&service->lock);
		return -EBUSY;
	}

	if (!service->stage_open && service->status.state == PERSISTED_OTA_STATE_IDLE) {
		k_mutex_unlock(&service->lock);
		return -EALREADY;
	}

	if (service->stage_open) {
		ota_service_close_stage_context(service);
	}

	ret = ota_service_update_state_locked(service,
					      PERSISTED_OTA_STATE_IDLE,
					      NULL,
					      NULL);
	k_mutex_unlock(&service->lock);
	return ret;
}

int ota_service_request_apply(struct ota_service *service)
{
	struct persisted_ota_version staged_version;
	struct persisted_ota_attempt attempt;
	uint32_t bytes_written;
	int ret;
	int update_ret;

	if (service == NULL) {
		return -EINVAL;
	}

	bytes_written = service->app_context->persisted_config.ota.last_attempt.bytes_written;

	k_mutex_lock(&service->lock, K_FOREVER);
	if (service->stage_open) {
		k_mutex_unlock(&service->lock);
		return -EBUSY;
	}

	if (service->status.state == PERSISTED_OTA_STATE_APPLY_REQUESTED) {
		k_mutex_unlock(&service->lock);
		return -EALREADY;
	}

	if (service->status.state != PERSISTED_OTA_STATE_STAGED) {
		k_mutex_unlock(&service->lock);
		return -EAGAIN;
	}

	staged_version = service->status.staged_version;
	if (!staged_version.available) {
		ret = ota_service_read_slot_version(PM_MCUBOOT_SECONDARY_ID, &staged_version);
		if (ret != 0) {
			attempt = ota_make_attempt(
				PERSISTED_OTA_LAST_RESULT_REJECTED_INVALID_IMAGE,
				ret,
				bytes_written,
				NULL);
			(void)ota_service_update_state_locked(service, PERSISTED_OTA_STATE_IDLE,
							      NULL, &attempt);
			k_mutex_unlock(&service->lock);
			return ret;
		}
	}

	ret = boot_request_upgrade(false);
	attempt = ota_make_attempt(ret == 0 ?
				PERSISTED_OTA_LAST_RESULT_APPLY_REQUESTED :
				PERSISTED_OTA_LAST_RESULT_APPLY_REQUEST_FAILED,
			 ret,
			 bytes_written,
			 &staged_version);
	update_ret = ota_service_update_state_locked(
		service,
		ret == 0 ? PERSISTED_OTA_STATE_APPLY_REQUESTED :
			   PERSISTED_OTA_STATE_STAGED,
		&staged_version,
		&attempt);
	k_mutex_unlock(&service->lock);
	return update_ret != 0 ? update_ret : ret;
}
