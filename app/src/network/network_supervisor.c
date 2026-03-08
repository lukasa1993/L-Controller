#include <errno.h>

#include <zephyr/logging/log.h>

#include "network/network_supervisor.h"

#include "network/reachability.h"
#include "network/wifi_lifecycle.h"

LOG_MODULE_DECLARE(app, CONFIG_LOG_DEFAULT_LEVEL);

static void network_supervisor_update_connectivity_state(struct network_runtime_state *network_state)
{
	if (!network_state->wifi_ready) {
		network_state->connectivity_state = NETWORK_CONNECTIVITY_NOT_READY;
		return;
	}

	if (network_state->wifi_connected && network_state->ipv4_bound) {
		if (!network_state->reachability_checked) {
			network_state->connectivity_state = NETWORK_CONNECTIVITY_CONNECTING;
			return;
		}

		network_state->connectivity_state = network_state->reachability_ok
						      ? NETWORK_CONNECTIVITY_HEALTHY
						      : NETWORK_CONNECTIVITY_LAN_UP_UPSTREAM_DEGRADED;
		return;
	}

	if (network_state->connect_status == -EINPROGRESS) {
		network_state->connectivity_state = NETWORK_CONNECTIVITY_CONNECTING;
		return;
	}

	if (wifi_lifecycle_has_link_loss(network_state) || network_state->last_failure.recorded) {
		network_state->connectivity_state = NETWORK_CONNECTIVITY_DEGRADED_RETRYING;
		return;
	}

	network_state->connectivity_state = NETWORK_CONNECTIVITY_CONNECTING;
}

static void network_supervisor_record_failure(struct network_runtime_state *network_state,
					      enum network_failure_stage stage, int reason)
{
	network_state->last_failure.recorded = true;
	network_state->last_failure.failure_stage = stage;
	network_state->last_failure.reason = reason;
}

static enum network_failure_stage network_supervisor_startup_failure_stage(
	const struct network_runtime_state *network_state)
{
	if (network_state->wifi_connected) {
		return NETWORK_FAILURE_STAGE_DHCP;
	}

	return NETWORK_FAILURE_STAGE_CONNECT;
}

void network_supervisor_init(struct network_runtime_state *network_state, struct net_if *wifi_iface)
{
	if (network_state == NULL) {
		return;
	}

	wifi_lifecycle_init(network_state, wifi_iface);
	network_state->connectivity_state = NETWORK_CONNECTIVITY_NOT_READY;
	network_state->last_reachability_status = -EAGAIN;
}

int network_supervisor_start(struct network_runtime_state *network_state,
			     const struct app_config *config)
{
	int ret;

	if (network_state == NULL || config == NULL) {
		return -EINVAL;
	}

	ret = wifi_lifecycle_register_callbacks(network_state);
	if (ret != 0) {
		network_supervisor_record_failure(network_state, NETWORK_FAILURE_STAGE_READY, ret);
		network_supervisor_update_connectivity_state(network_state);
		return ret;
	}

	ret = wifi_lifecycle_wait_for_ready(network_state);
	if (ret != 0) {
		network_supervisor_record_failure(network_state, NETWORK_FAILURE_STAGE_READY, ret);
		network_supervisor_update_connectivity_state(network_state);
		return ret;
	}

	ret = wifi_lifecycle_connect_once(network_state, &config->wifi);
	if (ret != 0) {
		network_supervisor_record_failure(network_state, NETWORK_FAILURE_STAGE_CONNECT, ret);
		network_supervisor_update_connectivity_state(network_state);
		return ret;
	}

	network_supervisor_update_connectivity_state(network_state);

	ret = wifi_lifecycle_wait_for_connection_and_ipv4(network_state, config->wifi.timeout_ms);
	if (ret != 0) {
		if (!network_state->wifi_connected) {
			network_state->connect_status = ret;
		}

		network_supervisor_record_failure(network_state,
						  network_supervisor_startup_failure_stage(network_state),
						  ret);
		network_supervisor_update_connectivity_state(network_state);
		return ret;
	}

	ret = reachability_check_host(&config->reachability);
	network_state->reachability_checked = true;
	network_state->reachability_ok = (ret == 0);
	network_state->last_reachability_status = ret;

	if (ret != 0) {
		network_supervisor_record_failure(network_state, NETWORK_FAILURE_STAGE_REACHABILITY,
						  ret);
		network_supervisor_update_connectivity_state(network_state);
		return ret;
	}

	network_supervisor_update_connectivity_state(network_state);
	LOG_INF("Network supervisor state=%s",
		network_supervisor_connectivity_state_text(network_state->connectivity_state));
	return 0;
}

int network_supervisor_get_status(struct network_runtime_state *network_state,
				  struct network_supervisor_status *status)
{
	if (network_state == NULL || status == NULL) {
		return -EINVAL;
	}

	network_supervisor_update_connectivity_state(network_state);

	*status = (struct network_supervisor_status){
		.connectivity_state = network_state->connectivity_state,
		.wifi_ready = network_state->wifi_ready,
		.wifi_connected = network_state->wifi_connected,
		.ipv4_bound = network_state->ipv4_bound,
		.reachability_ok = network_state->reachability_ok,
		.leased_ipv4 = network_state->leased_ipv4,
		.last_failure = network_state->last_failure,
	};

	return 0;
}

const char *network_supervisor_connectivity_state_text(enum network_connectivity_state state)
{
	switch (state) {
	case NETWORK_CONNECTIVITY_NOT_READY:
		return "not-ready";
	case NETWORK_CONNECTIVITY_CONNECTING:
		return "connecting";
	case NETWORK_CONNECTIVITY_HEALTHY:
		return "healthy";
	case NETWORK_CONNECTIVITY_LAN_UP_UPSTREAM_DEGRADED:
		return "lan-up-upstream-degraded";
	case NETWORK_CONNECTIVITY_DEGRADED_RETRYING:
		return "degraded-retrying";
	default:
		return "unknown";
	}
}

const char *network_supervisor_failure_stage_text(enum network_failure_stage stage)
{
	switch (stage) {
	case NETWORK_FAILURE_STAGE_NONE:
		return "none";
	case NETWORK_FAILURE_STAGE_READY:
		return "ready";
	case NETWORK_FAILURE_STAGE_CONNECT:
		return "connect";
	case NETWORK_FAILURE_STAGE_DHCP:
		return "dhcp";
	case NETWORK_FAILURE_STAGE_REACHABILITY:
		return "reachability";
	case NETWORK_FAILURE_STAGE_DISCONNECT:
		return "disconnect";
	default:
		return "unknown";
	}
}
