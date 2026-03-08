#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/net_mgmt.h>

struct app_config;
struct recovery_manager;

enum network_connectivity_state {
	NETWORK_CONNECTIVITY_NOT_READY,
	NETWORK_CONNECTIVITY_CONNECTING,
	NETWORK_CONNECTIVITY_HEALTHY,
	NETWORK_CONNECTIVITY_LAN_UP_UPSTREAM_DEGRADED,
	NETWORK_CONNECTIVITY_DEGRADED_RETRYING,
};

enum network_failure_stage {
	NETWORK_FAILURE_STAGE_NONE,
	NETWORK_FAILURE_STAGE_READY,
	NETWORK_FAILURE_STAGE_CONNECT,
	NETWORK_FAILURE_STAGE_DHCP,
	NETWORK_FAILURE_STAGE_REACHABILITY,
	NETWORK_FAILURE_STAGE_DISCONNECT,
};

struct network_failure_record {
	bool recorded;
	enum network_failure_stage failure_stage;
	int reason;
};

struct network_runtime_state {
	struct net_if *wifi_iface;
	const struct app_config *config;
	struct recovery_manager *recovery;
	struct net_mgmt_event_callback wifi_event_callback;
	struct net_mgmt_event_callback ipv4_event_callback;
	struct k_sem wifi_ready_sem;
	struct k_sem connect_sem;
	struct k_sem ipv4_sem;
	struct k_sem startup_outcome_sem;
	struct k_work_delayable supervisor_retry_work;
	bool wifi_ready;
	bool wifi_connected;
	bool ipv4_bound;
	bool reachability_checked;
	bool reachability_ok;
	bool disconnect_seen;
	bool ipv4_lease_lost;
	bool startup_settled;
	bool startup_wait_pending;
	bool bringup_in_progress;
	int connect_status;
	int last_disconnect_status;
	int last_reachability_status;
	int32_t retry_interval_ms;
	int64_t startup_deadline_ms;
	int64_t bringup_deadline_ms;
	int64_t next_retry_at_ms;
	enum network_connectivity_state connectivity_state;
	struct network_failure_record last_failure;
	struct in_addr leased_ipv4;
};
