#pragma once

#include <stdbool.h>
#include <stdint.h>

#define PERSISTED_AUTH_USERNAME_MAX_LEN 32
#define PERSISTED_AUTH_PASSWORD_MAX_LEN 64
#define PERSISTED_ACTION_ID_MAX_LEN 24
#define PERSISTED_ACTION_MAX_COUNT 8
#define PERSISTED_OTA_GITHUB_OWNER_MAX_LEN 32
#define PERSISTED_OTA_GITHUB_REPO_MAX_LEN 32
#define PERSISTED_SCHEDULE_ID_MAX_LEN 24
#define PERSISTED_SCHEDULE_MAX_COUNT 8
#define PERSISTED_SCHEDULE_CRON_EXPRESSION_MAX_LEN 64

enum persistence_section {
	PERSISTENCE_SECTION_AUTH = 0,
	PERSISTENCE_SECTION_ACTIONS,
	PERSISTENCE_SECTION_RELAY,
	PERSISTENCE_SECTION_SCHEDULE,
	PERSISTENCE_SECTION_OTA,
};

enum persistence_load_state {
	PERSISTENCE_LOAD_STATE_LOADED = 0,
	PERSISTENCE_LOAD_STATE_EMPTY_DEFAULT,
	PERSISTENCE_LOAD_STATE_INVALID_RESET,
	PERSISTENCE_LOAD_STATE_INCOMPATIBLE_RESET,
};

enum persistence_migration_action {
	PERSISTENCE_MIGRATION_ACTION_NONE = 0,
	PERSISTENCE_MIGRATION_ACTION_RESET_TO_DEFAULTS,
	PERSISTENCE_MIGRATION_ACTION_RESEED_FROM_CONFIG,
};

enum persisted_relay_reboot_policy {
	PERSISTED_RELAY_REBOOT_POLICY_SAFE_OFF = 0,
	PERSISTED_RELAY_REBOOT_POLICY_RESTORE_LAST_DESIRED = 1,
	PERSISTED_RELAY_REBOOT_POLICY_IGNORE_LAST_DESIRED = 2,
};

enum persisted_ota_state {
	PERSISTED_OTA_STATE_IDLE = 0,
	PERSISTED_OTA_STATE_STAGING,
	PERSISTED_OTA_STATE_STAGED,
	PERSISTED_OTA_STATE_APPLY_REQUESTED,
	PERSISTED_OTA_STATE_PENDING_CONFIRM,
};

enum persisted_ota_last_result_code {
	PERSISTED_OTA_LAST_RESULT_NONE = 0,
	PERSISTED_OTA_LAST_RESULT_STAGE_READY,
	PERSISTED_OTA_LAST_RESULT_STAGE_FAILED,
	PERSISTED_OTA_LAST_RESULT_APPLY_REQUESTED,
	PERSISTED_OTA_LAST_RESULT_APPLY_REQUEST_FAILED,
	PERSISTED_OTA_LAST_RESULT_REJECTED_SAME_VERSION,
	PERSISTED_OTA_LAST_RESULT_REJECTED_DOWNGRADE,
	PERSISTED_OTA_LAST_RESULT_REJECTED_INVALID_IMAGE,
	PERSISTED_OTA_LAST_RESULT_REMOTE_UP_TO_DATE,
	PERSISTED_OTA_LAST_RESULT_REMOTE_CHECK_FAILED,
	PERSISTED_OTA_LAST_RESULT_CONFIRMED,
	PERSISTED_OTA_LAST_RESULT_ROLLED_BACK,
};

struct persisted_auth {
	uint32_t schema_version;
	char username[PERSISTED_AUTH_USERNAME_MAX_LEN];
	char password[PERSISTED_AUTH_PASSWORD_MAX_LEN];
};

struct persisted_action {
	char action_id[PERSISTED_ACTION_ID_MAX_LEN];
	bool enabled;
	bool relay_on;
};

struct persisted_action_catalog {
	uint32_t schema_version;
	uint32_t count;
	struct persisted_action entries[PERSISTED_ACTION_MAX_COUNT];
};

struct persisted_relay {
	uint32_t schema_version;
	bool last_desired_state;
	enum persisted_relay_reboot_policy reboot_policy;
};

struct persisted_ota_version {
	bool available;
	uint8_t major;
	uint8_t minor;
	uint16_t revision;
	uint32_t build_num;
	uint32_t image_size;
};

struct persisted_ota_attempt {
	bool recorded;
	enum persisted_ota_last_result_code result;
	int32_t error_code;
	uint32_t bytes_written;
	bool rollback_detected;
	int32_t rollback_reason;
	struct persisted_ota_version version;
};

struct persisted_ota_remote_policy {
	bool auto_update_enabled;
	uint32_t check_interval_hours;
	char github_owner[PERSISTED_OTA_GITHUB_OWNER_MAX_LEN];
	char github_repo[PERSISTED_OTA_GITHUB_REPO_MAX_LEN];
};

struct persisted_ota {
	uint32_t schema_version;
	enum persisted_ota_state state;
	struct persisted_ota_version staged_version;
	struct persisted_ota_attempt last_attempt;
	struct persisted_ota_remote_policy remote_policy;
};

struct persisted_schedule {
	char schedule_id[PERSISTED_SCHEDULE_ID_MAX_LEN];
	char action_id[PERSISTED_ACTION_ID_MAX_LEN];
	bool enabled;
	char cron_expression[PERSISTED_SCHEDULE_CRON_EXPRESSION_MAX_LEN];
};

struct persisted_schedule_table {
	uint32_t schema_version;
	uint32_t count;
	struct persisted_schedule entries[PERSISTED_SCHEDULE_MAX_COUNT];
};

struct persisted_auth_save_request {
	char username[PERSISTED_AUTH_USERNAME_MAX_LEN];
	char password[PERSISTED_AUTH_PASSWORD_MAX_LEN];
};

struct persisted_action_catalog_save_request {
	uint32_t count;
	struct persisted_action entries[PERSISTED_ACTION_MAX_COUNT];
};

struct persisted_relay_save_request {
	bool last_desired_state;
	enum persisted_relay_reboot_policy reboot_policy;
};

struct persisted_ota_save_request {
	enum persisted_ota_state state;
	struct persisted_ota_version staged_version;
	struct persisted_ota_attempt last_attempt;
	struct persisted_ota_remote_policy remote_policy;
};

struct persisted_schedule_table_save_request {
	uint32_t count;
	struct persisted_schedule entries[PERSISTED_SCHEDULE_MAX_COUNT];
};

struct persisted_config_save_request {
	bool has_auth;
	struct persisted_auth_save_request auth;
	bool has_actions;
	struct persisted_action_catalog_save_request actions;
	bool has_relay;
	struct persisted_relay_save_request relay;
	bool has_schedule;
	struct persisted_schedule_table_save_request schedule;
	bool has_ota;
	struct persisted_ota_save_request ota;
};

struct persistence_section_status {
	enum persistence_section section;
	enum persistence_load_state state;
	enum persistence_migration_action migration_action;
	uint32_t stored_schema_version;
	uint32_t expected_schema_version;
	bool reseeded;
};

struct persistence_load_report {
	struct persistence_section_status auth;
	struct persistence_section_status actions;
	struct persistence_section_status relay;
	struct persistence_section_status schedule;
	struct persistence_section_status ota;
};

struct persisted_config {
	uint32_t layout_version;
	struct persisted_auth auth;
	struct persisted_action_catalog actions;
	struct persisted_relay relay;
	struct persisted_schedule_table schedule;
	struct persisted_ota ota;
	struct persistence_load_report load_report;
};
