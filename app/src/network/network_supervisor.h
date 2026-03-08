#pragma once

#include <zephyr/net/net_if.h>

#include "app/app_config.h"
#include "network/network_state.h"

struct recovery_manager;

struct network_supervisor_status {
	enum network_connectivity_state connectivity_state;
	bool wifi_ready;
	bool wifi_connected;
	bool ipv4_bound;
	bool reachability_ok;
	int last_reachability_status;
	struct in_addr leased_ipv4;
	struct network_failure_record last_failure;
};

void network_supervisor_init(struct network_runtime_state *network_state,
			     struct net_if *wifi_iface,
			     struct recovery_manager *recovery);
int network_supervisor_start(struct network_runtime_state *network_state,
			     const struct app_config *config);
int network_supervisor_get_status(struct network_runtime_state *network_state,
				  struct network_supervisor_status *status);
const char *network_supervisor_connectivity_state_text(enum network_connectivity_state state);
const char *network_supervisor_failure_stage_text(enum network_failure_stage stage);
