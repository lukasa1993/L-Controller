#pragma once

#include <stdint.h>

#include <zephyr/net/wifi_mgmt.h>

#include "persistence/persistence_types.h"

#define APP_PANEL_LOGIN_COOLDOWN_MS CONFIG_APP_PANEL_LOGIN_COOLDOWN_MS
#define APP_PANEL_LOGIN_FAILURE_LIMIT CONFIG_APP_PANEL_LOGIN_FAILURE_LIMIT
#define APP_PANEL_MAX_SESSIONS CONFIG_APP_PANEL_MAX_SESSIONS
#define APP_PANEL_PORT CONFIG_APP_PANEL_PORT
#define APP_PERSISTENCE_LAYOUT_VERSION CONFIG_APP_PERSISTENCE_LAYOUT_VERSION
#define APP_SCHEDULER_CADENCE_SECONDS CONFIG_APP_SCHEDULER_CADENCE_SECONDS
#define APP_SCHEDULER_PROBLEM_HISTORY_CAPACITY 10U
#define APP_SCHEDULER_TRUSTED_CLOCK_TIMEOUT_MS                                     \
	CONFIG_APP_SCHEDULER_TRUSTED_CLOCK_TIMEOUT_MS
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
	uint32_t problem_history_capacity;
};

struct app_config {
	const char *board_name;
	struct app_wifi_config wifi;
	struct app_reachability_config reachability;
	struct app_recovery_config recovery;
	struct app_panel_config panel;
	struct app_persistence_config persistence;
	struct app_scheduler_config scheduler;
};
