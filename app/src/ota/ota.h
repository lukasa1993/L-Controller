#pragma once

#include <zephyr/dfu/flash_img.h>
#include <zephyr/kernel.h>

#include <net/downloader.h>

#include "persistence/persistence_types.h"

struct app_context;

enum ota_remote_job_state {
	OTA_REMOTE_JOB_IDLE = 0,
	OTA_REMOTE_JOB_CHECKING,
	OTA_REMOTE_JOB_DOWNLOADING,
	OTA_REMOTE_JOB_APPLYING,
};

struct ota_runtime_status {
	bool implemented;
	bool image_confirmed;
	bool remote_busy;
	enum ota_remote_job_state remote_state;
	struct persisted_ota_version current_version;
	enum persisted_ota_state state;
	struct persisted_ota_version staged_version;
	struct persisted_ota_attempt last_attempt;
	struct persisted_ota_remote_policy remote_policy;
};

struct ota_service {
	struct app_context *app_context;
	struct k_mutex lock;
	struct flash_img_context stage_context;
	struct k_work_delayable remote_work;
	struct k_work_delayable reboot_work;
	struct k_sem remote_done_sem;
	struct downloader remote_downloader;
	struct ota_runtime_status status;
	int remote_download_result;
	char remote_download_buffer[2048];
	bool remote_manual_request;
	bool remote_downloader_ready;
	bool stage_open;
};

int ota_service_init(struct ota_service *service, struct app_context *app_context);
const struct ota_runtime_status *ota_service_get_status(const struct ota_service *service);
int ota_service_copy_snapshot(const struct ota_service *service,
			      struct ota_runtime_status *status_out);
int ota_service_begin_staging(struct ota_service *service);
int ota_service_write_chunk(struct ota_service *service,
			    const uint8_t *data,
			    size_t data_len);
int ota_service_abort_staging(struct ota_service *service, int error_code);
int ota_service_finish_staging(struct ota_service *service);
int ota_service_clear_staged_image(struct ota_service *service);
int ota_service_request_apply(struct ota_service *service);
int ota_service_request_remote_update(struct ota_service *service);
