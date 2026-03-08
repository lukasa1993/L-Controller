#pragma once

#include <stdbool.h>

#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/net_mgmt.h>

struct network_runtime_state {
	struct net_if *wifi_iface;
	struct net_mgmt_event_callback wifi_event_callback;
	struct net_mgmt_event_callback ipv4_event_callback;
	struct k_sem wifi_ready_sem;
	struct k_sem connect_sem;
	struct k_sem ipv4_sem;
	bool wifi_ready;
	bool wifi_connected;
	bool ipv4_bound;
	int connect_status;
	int last_disconnect_status;
	struct in_addr leased_ipv4;
};
