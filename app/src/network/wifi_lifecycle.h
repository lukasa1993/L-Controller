#pragma once

#include <stdint.h>

#include "app/app_config.h"
#include "network/network_state.h"

void wifi_lifecycle_init(struct network_runtime_state *network_state, struct net_if *wifi_iface);
int wifi_lifecycle_security_from_text(const char *security_text,
			      enum wifi_security_type *security);
int wifi_lifecycle_register_callbacks(struct network_runtime_state *network_state);
int wifi_lifecycle_wait_for_ready(struct network_runtime_state *network_state);
int wifi_lifecycle_connect_once(struct network_runtime_state *network_state,
				const struct app_wifi_config *wifi_config);
int wifi_lifecycle_wait_for_connection_and_ipv4(struct network_runtime_state *network_state,
					     int32_t timeout_ms);
bool wifi_lifecycle_has_link_loss(const struct network_runtime_state *network_state);
