#pragma once

#include <stdint.h>

#include <zephyr/net/wifi_mgmt.h>

#include "persistence/persistence_types.h"

#define APP_PANEL_LOGIN_COOLDOWN_MS CONFIG_APP_PANEL_LOGIN_COOLDOWN_MS
#define APP_PANEL_LOGIN_FAILURE_LIMIT CONFIG_APP_PANEL_LOGIN_FAILURE_LIMIT
#define APP_PANEL_MAX_SESSIONS CONFIG_APP_PANEL_MAX_SESSIONS
#define APP_PANEL_PORT CONFIG_APP_PANEL_PORT
#define APP_OTA_CONFIRM_STABLE_WINDOW_MS CONFIG_APP_OTA_CONFIRM_STABLE_WINDOW_MS
#define APP_OTA_REMOTE_DEFAULT_CHECK_INTERVAL_HOURS                               \
	CONFIG_APP_OTA_REMOTE_DEFAULT_CHECK_INTERVAL_HOURS
#define APP_OTA_REMOTE_GITHUB_OWNER CONFIG_APP_OTA_REMOTE_GITHUB_OWNER
#define APP_OTA_REMOTE_GITHUB_REPO CONFIG_APP_OTA_REMOTE_GITHUB_REPO
#define APP_PERSISTENCE_LAYOUT_VERSION CONFIG_APP_PERSISTENCE_LAYOUT_VERSION
#define APP_SCHEDULER_CADENCE_SECONDS CONFIG_APP_SCHEDULER_CADENCE_SECONDS
#define APP_SCHEDULER_PROBLEM_HISTORY_CAPACITY 10U
#define APP_SCHEDULER_TRUSTED_CLOCK_TIMEOUT_MS                                     \
	CONFIG_APP_SCHEDULER_TRUSTED_CLOCK_TIMEOUT_MS
#define APP_SCHEDULER_TRUSTED_CLOCK_SERVER CONFIG_APP_SCHEDULER_TRUSTED_CLOCK_SERVER
#define APP_RELAY_REBOOT_POLICY_DEFAULT                                            \
	((enum persisted_relay_reboot_policy)CONFIG_APP_RELAY_REBOOT_POLICY_DEFAULT)

struct app_wifi_config {
	const char *ssid;
	const char *psk;
	enum wifi_security_type security;
	int32_t timeout_ms;
	int32_t retry_interval_ms;
};

struct app_reachability_config {
	const char *host;
	uint16_t port;
};

struct app_recovery_config {
	int32_t watchdog_timeout_ms;
	int32_t degraded_patience_ms;
	int32_t stable_window_ms;
	int32_t cooldown_ms;
};

struct app_persistence_config {
	uint32_t layout_version;
	enum persisted_relay_reboot_policy default_relay_reboot_policy;
};

struct app_panel_config {
	uint16_t port;
	uint32_t max_sessions;
	uint32_t login_failure_limit;
	int32_t login_cooldown_ms;
};

struct app_scheduler_config {
	uint32_t cadence_seconds;
	int32_t trusted_clock_timeout_ms;
	const char *trusted_clock_server;
	uint32_t problem_history_capacity;
};

struct app_ota_config {
	int32_t confirm_stable_window_ms;
	uint32_t remote_default_check_interval_hours;
	const char *remote_github_owner;
	const char *remote_github_repo;
};

struct app_config {
	const char *board_name;
	struct app_wifi_config wifi;
	struct app_reachability_config reachability;
	struct app_recovery_config recovery;
	struct app_ota_config ota;
	struct app_panel_config panel;
	struct app_persistence_config persistence;
	struct app_scheduler_config scheduler;
};
