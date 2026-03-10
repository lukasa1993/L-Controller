#pragma once

#include <zephyr/dfu/flash_img.h>
#include <zephyr/kernel.h>

#include "persistence/persistence_types.h"

struct app_context;

struct ota_runtime_status {
	bool implemented;
	bool image_confirmed;
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
	struct ota_runtime_status status;
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
