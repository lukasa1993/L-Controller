#pragma once

#include <stdint.h>

#include <zephyr/net/wifi_mgmt.h>

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

struct app_config {
	const char *board_name;
	struct app_wifi_config wifi;
	struct app_reachability_config reachability;
	struct app_recovery_config recovery;
};
