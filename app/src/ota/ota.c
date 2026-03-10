#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pm_config.h>

#include <zephyr/net/http/client.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/tls_credentials.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/logging/log.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/sys/util.h>

#include "app/app_context.h"
#include "network/network_supervisor.h"
#include "ota/ota.h"
#include "ota/ota_github_ca_certs.h"
#include "persistence/persistence.h"

LOG_MODULE_REGISTER(ota, CONFIG_LOG_DEFAULT_LEVEL);

#define OTA_GITHUB_API_HOST "api.github.com"
#define OTA_GITHUB_RELEASE_PATH_MAX 160
#define OTA_GITHUB_RELEASE_JSON_MAX 8192
#define OTA_GITHUB_RELEASE_URL_MAX 384
#define OTA_GITHUB_ASSET_NAME_MAX 96
#define OTA_GITHUB_TAG_NAME_MAX 48
#define OTA_GITHUB_METADATA_TIMEOUT_MS 15000
#define OTA_GITHUB_PRIMARY_ASSET "app_update.bin"
#define OTA_GITHUB_FALLBACK_ASSET "zephyr.signed.bin"
#define OTA_GITHUB_USER_AGENT "LNH-Nordic/1.0"
#define OTA_REMOTE_REBOOT_DELAY_MS 1200
#define OTA_TLS_TAG_USERTRUST 8101
#define OTA_TLS_TAG_ISRG 8102

static struct ota_service *ota_singleton_service;
static const sec_tag_t ota_tls_sec_tag_list[] = {
	OTA_TLS_TAG_USERTRUST,
	OTA_TLS_TAG_ISRG,
};

struct ota_github_release_fetch {
	char body[OTA_GITHUB_RELEASE_JSON_MAX];
	size_t body_len;
	uint16_t status_code;
	bool overflow;
};

struct ota_github_release_info {
	char tag_name[OTA_GITHUB_TAG_NAME_MAX];
	char asset_name[OTA_GITHUB_ASSET_NAME_MAX];
	char asset_url[OTA_GITHUB_RELEASE_URL_MAX];
};

static void ota_service_schedule_next_auto_check(struct ota_service *service, k_timeout_t delay);
static void ota_service_set_remote_state_locked(struct ota_service *service,
					       enum ota_remote_job_state remote_state,
					       bool remote_busy);
static int ota_service_fetch_github_release(struct ota_service *service,
					    struct ota_github_release_info *release_out);
static int ota_service_run_remote_update(struct ota_service *service, bool manual_trigger);
static void ota_service_remote_work_handler(struct k_work *work);
static void ota_service_reboot_work_handler(struct k_work *work);

static void ota_version_clear(struct persisted_ota_version *version)
{
	memset(version, 0, sizeof(*version));
}

static void ota_attempt_clear(struct persisted_ota_attempt *attempt)
{
	memset(attempt, 0, sizeof(*attempt));
	attempt->result = PERSISTED_OTA_LAST_RESULT_NONE;
}

static bool ota_string_ends_with(const char *value, const char *suffix)
{
	size_t value_len;
	size_t suffix_len;

	if (value == NULL || suffix == NULL) {
		return false;
	}

	value_len = strlen(value);
	suffix_len = strlen(suffix);
	return value_len >= suffix_len &&
	       strcmp(value + value_len - suffix_len, suffix) == 0;
}

static int ota_service_add_tls_credential(sec_tag_t tag, const char *certificate)
{
	int ret;

	ret = tls_credential_add(tag,
				 TLS_CREDENTIAL_CA_CERTIFICATE,
				 certificate,
				 strlen(certificate) + 1U);
	if (ret == -EEXIST) {
		return 0;
	}

	return ret;
}

static int ota_service_register_tls_credentials(void)
{
	int ret;

	ret = ota_service_add_tls_credential(OTA_TLS_TAG_USERTRUST,
						 ota_github_usertrust_root_ca);
	if (ret != 0) {
		return ret;
	}

	return ota_service_add_tls_credential(OTA_TLS_TAG_ISRG, ota_github_isrg_root_x1_ca);
}

static int ota_service_downloader_callback(const struct downloader_evt *event)
{
	struct ota_service *service = ota_singleton_service;
	int ret;

	if (service == NULL || event == NULL) {
		return -EINVAL;
	}

	switch (event->id) {
	case DOWNLOADER_EVT_FRAGMENT:
		ret = ota_service_write_chunk(service, event->fragment.buf, event->fragment.len);
		if (ret != 0) {
			service->remote_download_result = ret;
			k_sem_give(&service->remote_done_sem);
		}
		return ret;
	case DOWNLOADER_EVT_DONE:
		service->remote_download_result = 0;
		k_sem_give(&service->remote_done_sem);
		return 0;
	case DOWNLOADER_EVT_ERROR:
		service->remote_download_result = event->error != 0 ? event->error : -EIO;
		k_sem_give(&service->remote_done_sem);
		return event->error == -ECONNRESET ? 0 : event->error;
	case DOWNLOADER_EVT_STOPPED:
		if (service->remote_download_result == 0) {
			service->remote_download_result = -ECANCELED;
		}
		k_sem_give(&service->remote_done_sem);
		return 0;
	case DOWNLOADER_EVT_DEINITIALIZED:
	default:
		return 0;
	}
}

static int ota_github_release_response_cb(struct http_response *response,
					  enum http_final_call final_data,
					  void *user_data)
{
	struct ota_github_release_fetch *fetch = user_data;

	ARG_UNUSED(final_data);

	if (response == NULL || fetch == NULL) {
		return -EINVAL;
	}

	fetch->status_code = response->http_status_code;
	if (response->body_frag_len == 0U) {
		return 0;
	}

	if (fetch->body_len + response->body_frag_len >= sizeof(fetch->body)) {
		fetch->overflow = true;
		return -E2BIG;
	}

	memcpy(fetch->body + fetch->body_len,
	       response->body_frag_start,
	       response->body_frag_len);
	fetch->body_len += response->body_frag_len;
	fetch->body[fetch->body_len] = '\0';
	return 0;
}

static int ota_service_connect_tls_socket(const char *host, const char *port, int *sock_out)
{
	struct addrinfo hints = { 0 };
	struct addrinfo *result = NULL;
	int sock = -1;
	int ret;

	if (host == NULL || port == NULL || sock_out == NULL) {
		return -EINVAL;
	}

	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	ret = getaddrinfo(host, port, &hints, &result);
	if (ret != 0 || result == NULL) {
		return -EHOSTUNREACH;
	}

	sock = socket(result->ai_family, result->ai_socktype, IPPROTO_TLS_1_2);
	if (sock < 0) {
		ret = -errno;
		freeaddrinfo(result);
		return ret;
	}

	ret = setsockopt(sock,
			 SOL_TLS,
			 TLS_SEC_TAG_LIST,
			 ota_tls_sec_tag_list,
			 sizeof(ota_tls_sec_tag_list));
	if (ret < 0) {
		ret = -errno;
		close(sock);
		freeaddrinfo(result);
		return ret;
	}

	ret = setsockopt(sock, SOL_TLS, TLS_HOSTNAME, host, strlen(host) + 1U);
	if (ret < 0) {
		ret = -errno;
		close(sock);
		freeaddrinfo(result);
		return ret;
	}

	ret = connect(sock, result->ai_addr, result->ai_addrlen);
	freeaddrinfo(result);
	if (ret < 0) {
		ret = -errno;
		close(sock);
		return ret;
	}

	*sock_out = sock;
	return 0;
}

static bool ota_json_copy_string(const char *cursor,
				 char *value_out,
				 size_t value_len,
				 const char **after_out)
{
	size_t offset = 0U;

	if (cursor == NULL || value_out == NULL || value_len == 0U) {
		return false;
	}

	while (*cursor != '\0') {
		if (*cursor == '"') {
			value_out[offset] = '\0';
			if (after_out != NULL) {
				*after_out = cursor + 1;
			}
			return true;
		}

		if (*cursor == '\\' && cursor[1] != '\0') {
			cursor++;
		}

		if (offset + 1U >= value_len) {
			return false;
		}

		value_out[offset++] = *cursor++;
	}

	return false;
}

static bool ota_json_find_string_value(const char *json,
				       const char *key,
				       char *value_out,
				       size_t value_len,
				       const char **after_out)
{
	char pattern[64];
	const char *match;
	int written;

	if (json == NULL || key == NULL || value_out == NULL) {
		return false;
	}

	written = snprintf(pattern, sizeof(pattern), "\"%s\":\"", key);
	if (written < 0 || (size_t)written >= sizeof(pattern)) {
		return false;
	}

	match = strstr(json, pattern);
	if (match == NULL) {
		return false;
	}

	return ota_json_copy_string(match + written, value_out, value_len, after_out);
}

static int ota_github_parse_latest_release(const char *json,
					   struct ota_github_release_info *release_out)
{
	char asset_name[OTA_GITHUB_ASSET_NAME_MAX];
	char asset_url[OTA_GITHUB_RELEASE_URL_MAX];
	const char *cursor;
	const char *after_name;
	int best_score = 0;

	if (json == NULL || release_out == NULL) {
		return -EINVAL;
	}

	memset(release_out, 0, sizeof(*release_out));
	if (!ota_json_find_string_value(json,
					"tag_name",
					release_out->tag_name,
					sizeof(release_out->tag_name),
					NULL)) {
		return -EBADMSG;
	}

	cursor = json;
	while ((cursor = strstr(cursor, "\"name\":\"")) != NULL) {
		int score = 0;

		cursor += strlen("\"name\":\"");
		if (!ota_json_copy_string(cursor,
					 asset_name,
					 sizeof(asset_name),
					 &after_name)) {
			return -EBADMSG;
		}

		if (!ota_json_find_string_value(after_name,
						"browser_download_url",
						asset_url,
						sizeof(asset_url),
						&cursor)) {
			continue;
		}

		if (strcmp(asset_name, OTA_GITHUB_PRIMARY_ASSET) == 0) {
			score = 3;
		} else if (strcmp(asset_name, OTA_GITHUB_FALLBACK_ASSET) == 0) {
			score = 2;
		} else if (ota_string_ends_with(asset_name, ".bin")) {
			score = 1;
		}

		if (score > best_score) {
			best_score = score;
			snprintf(release_out->asset_name,
				 sizeof(release_out->asset_name),
				 "%s",
				 asset_name);
			snprintf(release_out->asset_url,
				 sizeof(release_out->asset_url),
				 "%s",
				 asset_url);
		}
	}

	return best_score > 0 ? 0 : -ENOENT;
}

static int ota_version_from_release_tag(const char *tag,
					struct persisted_ota_version *version_out)
{
	const char *cursor;
	char *end = NULL;
	unsigned long major;
	unsigned long minor;
	unsigned long revision;
	unsigned long build = 0U;

	if (tag == NULL || version_out == NULL) {
		return -EINVAL;
	}

	memset(version_out, 0, sizeof(*version_out));
	cursor = (*tag == 'v' || *tag == 'V') ? tag + 1 : tag;
	major = strtoul(cursor, &end, 10);
	if (end == cursor || *end != '.') {
		return -EINVAL;
	}

	cursor = end + 1;
	minor = strtoul(cursor, &end, 10);
	if (end == cursor || *end != '.') {
		return -EINVAL;
	}

	cursor = end + 1;
	revision = strtoul(cursor, &end, 10);
	if (end == cursor) {
		return -EINVAL;
	}

	if (*end == '+') {
		cursor = end + 1;
		build = strtoul(cursor, &end, 10);
		if (end == cursor) {
			return -EINVAL;
		}
	}

	if (*end != '\0') {
		return -EINVAL;
	}

	version_out->available = true;
	version_out->major = (uint8_t)major;
	version_out->minor = (uint8_t)minor;
	version_out->revision = (uint16_t)revision;
	version_out->build_num = (uint32_t)build;
	return 0;
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
	enum ota_remote_job_state remote_state;
	bool remote_busy;

	remote_state = service->status.remote_state;
	remote_busy = service->status.remote_busy;

	service->status = (struct ota_runtime_status){
		.implemented = true,
		.remote_busy = remote_busy,
		.remote_state = remote_state,
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

static void ota_service_set_remote_state_locked(struct ota_service *service,
						enum ota_remote_job_state remote_state,
						bool remote_busy)
{
	if (service == NULL) {
		return;
	}

	service->status.remote_state = remote_state;
	service->status.remote_busy = remote_busy;
}

static int ota_service_record_remote_result_locked(
	struct ota_service *service,
	enum persisted_ota_last_result_code result,
	int error_code,
	uint32_t bytes_written,
	const struct persisted_ota_version *version)
{
	struct persisted_ota_attempt attempt = ota_make_attempt(result,
								error_code,
								bytes_written,
								version);

	return ota_service_update_state_locked(service, PERSISTED_OTA_STATE_IDLE, NULL, &attempt);
}

static void ota_service_schedule_next_auto_check(struct ota_service *service, k_timeout_t delay)
{
	if (service == NULL) {
		return;
	}

	k_work_reschedule(&service->remote_work, delay);
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
	struct downloader_cfg downloader_cfg = {
		.callback = ota_service_downloader_callback,
		.buf = service != NULL ? service->remote_download_buffer : NULL,
		.buf_size = service != NULL ? sizeof(service->remote_download_buffer) : 0U,
	};
	uint32_t interval_hours;
	if (service == NULL || app_context == NULL) {
		return -EINVAL;
	}

	memset(service, 0, sizeof(*service));
	service->app_context = app_context;
	ota_singleton_service = service;
	k_mutex_init(&service->lock);
	k_sem_init(&service->remote_done_sem, 0, 1);
	k_work_init_delayable(&service->remote_work, ota_service_remote_work_handler);
	k_work_init_delayable(&service->reboot_work, ota_service_reboot_work_handler);
	service->remote_download_result = 0;
	ota_service_sync_status_locked(service);
	if (!service->status.remote_policy.auto_update_enabled &&
	    service->status.remote_policy.github_owner[0] != '\0' &&
	    service->status.remote_policy.github_repo[0] != '\0') {
		struct persisted_ota persisted = service->app_context->persisted_config.ota;

		persisted.remote_policy.auto_update_enabled = true;
		k_mutex_lock(&service->lock, K_FOREVER);
		(void)ota_service_save_persisted_locked(service, &persisted);
		k_mutex_unlock(&service->lock);
	}

	downloader_cfg.buf = service->remote_download_buffer;
	downloader_cfg.buf_size = sizeof(service->remote_download_buffer);
	if (downloader_init(&service->remote_downloader, &downloader_cfg) == 0) {
		service->remote_downloader_ready = true;
	} else {
		LOG_WRN("Downloader init failed; remote OTA checks will stay unavailable");
	}

	if (ota_service_register_tls_credentials() != 0) {
		LOG_WRN("TLS credential registration failed; GitHub OTA checks will stay unavailable");
	}

	interval_hours = MAX(service->status.remote_policy.check_interval_hours, 1U);
	if (service->status.remote_policy.auto_update_enabled) {
		ota_service_schedule_next_auto_check(service, K_HOURS(interval_hours));
	}

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

static int ota_service_fetch_github_release(struct ota_service *service,
					    struct ota_github_release_info *release_out)
{
	struct ota_github_release_fetch fetch = { 0 };
	uint8_t recv_buf[1024];
	char path[OTA_GITHUB_RELEASE_PATH_MAX];
	int sock = -1;
	int ret;
	struct http_request request = { 0 };
	const char *headers[] = {
		"User-Agent: " OTA_GITHUB_USER_AGENT "\r\n",
		"Accept: application/vnd.github+json\r\n",
		"Connection: close\r\n",
		NULL,
	};

	if (service == NULL || release_out == NULL) {
		return -EINVAL;
	}

	ret = snprintf(path,
		       sizeof(path),
		       "/repos/%s/%s/releases/latest",
		       service->status.remote_policy.github_owner,
		       service->status.remote_policy.github_repo);
	if (ret < 0 || (size_t)ret >= sizeof(path)) {
		return -ENAMETOOLONG;
	}

	ret = ota_service_connect_tls_socket(OTA_GITHUB_API_HOST, "443", &sock);
	if (ret != 0) {
		return ret;
	}

	request.method = HTTP_GET;
	request.url = path;
	request.host = OTA_GITHUB_API_HOST;
	request.protocol = "HTTP/1.1";
	request.header_fields = headers;
	request.response = ota_github_release_response_cb;
	request.recv_buf = recv_buf;
	request.recv_buf_len = sizeof(recv_buf);

	ret = http_client_req(sock, &request, OTA_GITHUB_METADATA_TIMEOUT_MS, &fetch);
	close(sock);
	if (ret != 0) {
		return ret;
	}

	if (fetch.overflow) {
		return -E2BIG;
	}

	if (fetch.status_code != 200U) {
		return fetch.status_code == 404U ? -ENOENT : -EBADMSG;
	}

	return ota_github_parse_latest_release(fetch.body, release_out);
}

static int ota_service_run_remote_update(struct ota_service *service, bool manual_trigger)
{
	struct scheduler_runtime_status scheduler_status;
	struct network_supervisor_status network_status;
	struct ota_github_release_info release;
	struct persisted_ota_version release_version;
	struct downloader_host_cfg host_cfg = {
		.sec_tag_list = ota_tls_sec_tag_list,
		.sec_tag_count = ARRAY_SIZE(ota_tls_sec_tag_list),
		.family = AF_INET,
		.redirects_max = 4,
	};
	bool release_version_available = false;
	uint32_t bytes_written = 0U;
	int ret;

	if (service == NULL) {
		return -EINVAL;
	}

	k_mutex_lock(&service->lock, K_FOREVER);
	if (service->stage_open || service->status.state != PERSISTED_OTA_STATE_IDLE) {
		k_mutex_unlock(&service->lock);
		return -EBUSY;
	}

	if (!manual_trigger && !service->status.remote_policy.auto_update_enabled) {
		k_mutex_unlock(&service->lock);
		return 0;
	}
	k_mutex_unlock(&service->lock);

	if (!manual_trigger) {
		ret = scheduler_service_copy_snapshot(&service->app_context->scheduler,
						     &scheduler_status,
						     NULL,
						     0U,
						     NULL);
		if (ret != 0 ||
		    scheduler_status.clock_state != SCHEDULER_CLOCK_TRUST_STATE_TRUSTED) {
			return -EAGAIN;
		}
	}

	ret = network_supervisor_get_status(&service->app_context->network_state, &network_status);
	if (ret != 0 || network_status.connectivity_state != NETWORK_CONNECTIVITY_HEALTHY) {
		k_mutex_lock(&service->lock, K_FOREVER);
		(void)ota_service_record_remote_result_locked(
			service,
			PERSISTED_OTA_LAST_RESULT_REMOTE_CHECK_FAILED,
			ret != 0 ? ret : -ENETUNREACH,
			0U,
			NULL);
		k_mutex_unlock(&service->lock);
		return ret != 0 ? ret : -ENETUNREACH;
	}

	ret = ota_service_fetch_github_release(service, &release);
	if (ret != 0) {
		k_mutex_lock(&service->lock, K_FOREVER);
		(void)ota_service_record_remote_result_locked(
			service,
			PERSISTED_OTA_LAST_RESULT_REMOTE_CHECK_FAILED,
			ret,
			0U,
			NULL);
		k_mutex_unlock(&service->lock);
		return ret;
	}

	ret = ota_version_from_release_tag(release.tag_name, &release_version);
	if (ret == 0) {
		release_version_available = true;
		k_mutex_lock(&service->lock, K_FOREVER);
		if (service->status.current_version.available &&
		    ota_version_compare(&release_version, &service->status.current_version) <= 0) {
			(void)ota_service_record_remote_result_locked(
				service,
				PERSISTED_OTA_LAST_RESULT_REMOTE_UP_TO_DATE,
				0,
				0U,
				&release_version);
			k_mutex_unlock(&service->lock);
			return 0;
		}
		k_mutex_unlock(&service->lock);
	}

	k_mutex_lock(&service->lock, K_FOREVER);
	ota_service_set_remote_state_locked(service, OTA_REMOTE_JOB_DOWNLOADING, true);
	k_mutex_unlock(&service->lock);

	ret = ota_service_begin_staging(service);
	if (ret != 0) {
		k_mutex_lock(&service->lock, K_FOREVER);
		(void)ota_service_record_remote_result_locked(
			service,
			PERSISTED_OTA_LAST_RESULT_REMOTE_CHECK_FAILED,
			ret,
			0U,
			release_version_available ? &release_version : NULL);
		k_mutex_unlock(&service->lock);
		return ret;
	}

	service->remote_download_result = 0;
	k_sem_reset(&service->remote_done_sem);
	ret = downloader_get(&service->remote_downloader, &host_cfg, release.asset_url, 0U);
	if (ret != 0) {
		k_mutex_lock(&service->lock, K_FOREVER);
		bytes_written = flash_img_bytes_written(&service->stage_context);
		ota_service_close_stage_context(service);
		(void)ota_service_record_remote_result_locked(
			service,
			PERSISTED_OTA_LAST_RESULT_REMOTE_CHECK_FAILED,
			ret,
			bytes_written,
			release_version_available ? &release_version : NULL);
		k_mutex_unlock(&service->lock);
		return ret;
	}

	(void)k_sem_take(&service->remote_done_sem, K_FOREVER);
	ret = service->remote_download_result;
	if (ret != 0) {
		k_mutex_lock(&service->lock, K_FOREVER);
		bytes_written = service->stage_open ? flash_img_bytes_written(&service->stage_context) : 0U;
		if (service->stage_open) {
			ota_service_close_stage_context(service);
		}
		(void)ota_service_record_remote_result_locked(
			service,
			PERSISTED_OTA_LAST_RESULT_REMOTE_CHECK_FAILED,
			ret,
			bytes_written,
			release_version_available ? &release_version : NULL);
		k_mutex_unlock(&service->lock);
		return ret;
	}

	ret = ota_service_finish_staging(service);
	if (ret != 0) {
		return ret;
	}

	k_mutex_lock(&service->lock, K_FOREVER);
	ota_service_set_remote_state_locked(service, OTA_REMOTE_JOB_APPLYING, true);
	k_mutex_unlock(&service->lock);

	ret = ota_service_request_apply(service);
	if (ret == 0) {
		k_work_reschedule(&service->reboot_work, K_MSEC(OTA_REMOTE_REBOOT_DELAY_MS));
	}

	return ret;
}

static void ota_service_remote_work_handler(struct k_work *work)
{
	struct k_work_delayable *delayable = CONTAINER_OF(work, struct k_work_delayable, work);
	struct ota_service *service = CONTAINER_OF(delayable, struct ota_service, remote_work);
	uint32_t interval_hours;
	k_timeout_t next_delay;
	bool manual_trigger;
	bool auto_update_enabled;
	int ret;

	k_mutex_lock(&service->lock, K_FOREVER);
	manual_trigger = service->remote_manual_request;
	service->remote_manual_request = false;
	interval_hours = MAX(service->status.remote_policy.check_interval_hours, 1U);
	auto_update_enabled = service->status.remote_policy.auto_update_enabled;
	ota_service_set_remote_state_locked(service, OTA_REMOTE_JOB_CHECKING, true);
	k_mutex_unlock(&service->lock);

	ret = ota_service_run_remote_update(service, manual_trigger);

	k_mutex_lock(&service->lock, K_FOREVER);
	auto_update_enabled = service->status.remote_policy.auto_update_enabled;
	interval_hours = MAX(service->status.remote_policy.check_interval_hours, 1U);
	ota_service_set_remote_state_locked(service, OTA_REMOTE_JOB_IDLE, false);
	k_mutex_unlock(&service->lock);

	if (!auto_update_enabled) {
		return;
	}

	next_delay = K_HOURS(interval_hours);
	if (!manual_trigger &&
	    (ret == -EAGAIN || ret == -EBUSY || ret == -ENETUNREACH)) {
		next_delay = K_MINUTES(15);
	}

	ota_service_schedule_next_auto_check(service, next_delay);
}

static void ota_service_reboot_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	sys_reboot(SYS_REBOOT_COLD);
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

int ota_service_request_remote_update(struct ota_service *service)
{
	if (service == NULL) {
		return -EINVAL;
	}

	k_mutex_lock(&service->lock, K_FOREVER);
	if (service->status.remote_busy) {
		k_mutex_unlock(&service->lock);
		return -EALREADY;
	}

	if (service->stage_open || service->status.state != PERSISTED_OTA_STATE_IDLE) {
		k_mutex_unlock(&service->lock);
		return -EBUSY;
	}

	service->remote_manual_request = true;
	ota_service_set_remote_state_locked(service, OTA_REMOTE_JOB_CHECKING, true);
	k_mutex_unlock(&service->lock);

	ota_service_schedule_next_auto_check(service, K_NO_WAIT);
	return 0;
}
