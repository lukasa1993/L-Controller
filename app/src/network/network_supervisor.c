#include <errno.h>

#include <zephyr/logging/log.h>

#include "network/network_supervisor.h"

#include "network/reachability.h"
#include "network/wifi_lifecycle.h"

LOG_MODULE_DECLARE(app, CONFIG_LOG_DEFAULT_LEVEL);

static k_timeout_t remaining_timeout(int64_t deadline_ms)
{
	int64_t remaining_ms = deadline_ms - k_uptime_get();

	if (remaining_ms <= 0) {
		return K_NO_WAIT;
	}

	return K_MSEC(remaining_ms);
}

static void network_supervisor_set_connectivity_state(
	struct network_runtime_state *network_state,
	enum network_connectivity_state next_state)
{
	enum network_connectivity_state previous_state = network_state->connectivity_state;

	if (network_state->connectivity_state == next_state) {
		return;
	}

	network_state->connectivity_state = next_state;
	LOG_INF("Network supervisor state=%s",
		network_supervisor_connectivity_state_text(next_state));

	if (next_state == NETWORK_CONNECTIVITY_HEALTHY &&
	    previous_state != NETWORK_CONNECTIVITY_HEALTHY &&
	    network_state->last_failure.recorded) {
		LOG_INF("Network recovered to healthy; last failure=%s reason=%d",
			network_supervisor_failure_stage_text(
				network_state->last_failure.failure_stage),
			network_state->last_failure.reason);
	}
}

static void network_supervisor_update_connectivity_state(struct network_runtime_state *network_state)
{
	enum network_connectivity_state next_state;

	if (!network_state->wifi_ready) {
		next_state = network_state->startup_settled
				 ? NETWORK_CONNECTIVITY_DEGRADED_RETRYING
				 : NETWORK_CONNECTIVITY_NOT_READY;
		network_supervisor_set_connectivity_state(network_state, next_state);
		return;
	}

	if (network_state->wifi_connected && network_state->ipv4_bound) {
		if (!network_state->reachability_checked) {
			next_state = network_state->startup_settled
				     ? NETWORK_CONNECTIVITY_DEGRADED_RETRYING
				     : NETWORK_CONNECTIVITY_CONNECTING;
			network_supervisor_set_connectivity_state(network_state, next_state);
			return;
		}

		next_state = network_state->reachability_ok
				     ? NETWORK_CONNECTIVITY_HEALTHY
				     : NETWORK_CONNECTIVITY_LAN_UP_UPSTREAM_DEGRADED;
		network_supervisor_set_connectivity_state(network_state, next_state);
		return;
	}

	next_state = network_state->startup_settled ? NETWORK_CONNECTIVITY_DEGRADED_RETRYING
						     : NETWORK_CONNECTIVITY_CONNECTING;
	network_supervisor_set_connectivity_state(network_state, next_state);
}

static void network_supervisor_record_failure(struct network_runtime_state *network_state,
					      enum network_failure_stage stage, int reason)
{
	network_state->last_failure.recorded = true;
	network_state->last_failure.failure_stage = stage;
	network_state->last_failure.reason = reason;

	LOG_WRN("Network failure stage=%s reason=%d",
		network_supervisor_failure_stage_text(stage), reason);
}

static enum network_failure_stage network_supervisor_startup_failure_stage(
	const struct network_runtime_state *network_state)
{
	if (!network_state->wifi_ready) {
		return NETWORK_FAILURE_STAGE_READY;
	}

	if (network_state->reachability_checked && !network_state->reachability_ok) {
		return NETWORK_FAILURE_STAGE_REACHABILITY;
	}

	if (network_state->disconnect_seen) {
		return NETWORK_FAILURE_STAGE_DISCONNECT;
	}

	if (network_state->wifi_connected) {
		return NETWORK_FAILURE_STAGE_DHCP;
	}

	return NETWORK_FAILURE_STAGE_CONNECT;
}

static void network_supervisor_clear_retry_deadline(struct network_runtime_state *network_state)
{
	network_state->next_retry_at_ms = 0;
}

static void network_supervisor_arm_retry_deadline(struct network_runtime_state *network_state)
{
	if (network_state->retry_interval_ms <= 0 || network_state->next_retry_at_ms != 0) {
		return;
	}

	network_state->next_retry_at_ms = k_uptime_get() + network_state->retry_interval_ms;
}

static k_timeout_t network_supervisor_retry_delay(const struct network_runtime_state *network_state)
{
	if (network_state->next_retry_at_ms == 0) {
		return K_NO_WAIT;
	}

	return remaining_timeout(network_state->next_retry_at_ms);
}

static bool network_supervisor_retry_wait_active(const struct network_runtime_state *network_state)
{
	return network_state->next_retry_at_ms != 0 && k_uptime_get() < network_state->next_retry_at_ms;
}

static void network_supervisor_start_bringup(struct network_runtime_state *network_state)
{
	network_state->bringup_in_progress = true;
	network_state->bringup_deadline_ms = k_uptime_get() + network_state->retry_interval_ms;
}

static void network_supervisor_clear_bringup(struct network_runtime_state *network_state)
{
	network_state->bringup_in_progress = false;
	network_state->bringup_deadline_ms = 0;
}

static bool network_supervisor_bringup_expired(const struct network_runtime_state *network_state)
{
	return network_state->bringup_in_progress && k_uptime_get() >= network_state->bringup_deadline_ms;
}

static k_timeout_t network_supervisor_bringup_delay(const struct network_runtime_state *network_state)
{
	if (!network_state->bringup_in_progress) {
		return K_NO_WAIT;
	}

	return remaining_timeout(network_state->bringup_deadline_ms);
}

static void network_supervisor_complete_startup(struct network_runtime_state *network_state)
{
	if (network_state->startup_settled) {
		return;
	}

	network_state->startup_settled = true;

	if (!network_state->startup_wait_pending) {
		return;
	}

	network_state->startup_wait_pending = false;
	k_sem_give(&network_state->startup_outcome_sem);
}

static void network_supervisor_schedule_retry(struct network_runtime_state *network_state,
					      k_timeout_t delay)
{
	k_work_reschedule(&network_state->supervisor_retry_work, delay);
}

static struct network_runtime_state *network_supervisor_state_from_work(struct k_work *work)
{
	struct k_work_delayable *delayable = CONTAINER_OF(work, struct k_work_delayable, work);

	return CONTAINER_OF(delayable, struct network_runtime_state, supervisor_retry_work);
}

static struct app_wifi_config network_supervisor_retry_wifi_config(
	const struct network_runtime_state *network_state)
{
	struct app_wifi_config wifi_config = network_state->config->wifi;

	wifi_config.timeout_ms = network_state->retry_interval_ms;
	return wifi_config;
}

static void network_supervisor_retry_work_handler(struct k_work *work)
{
	struct network_runtime_state *network_state = network_supervisor_state_from_work(work);
	struct app_wifi_config wifi_config;
	int ret;

	if (network_state->config == NULL) {
		return;
	}

	if (network_state->bringup_in_progress && !network_state->wifi_connected &&
	    network_state->connect_status != -EINPROGRESS) {
		network_supervisor_clear_bringup(network_state);
		if (network_state->next_retry_at_ms == 0) {
			network_supervisor_record_failure(
				network_state,
				network_state->disconnect_seen ? NETWORK_FAILURE_STAGE_DISCONNECT
						       : NETWORK_FAILURE_STAGE_CONNECT,
				network_state->disconnect_seen
					? (network_state->last_disconnect_status
					   ? network_state->last_disconnect_status
					   : -ECONNRESET)
					: (network_state->connect_status ? network_state->connect_status : -EIO));
			network_supervisor_arm_retry_deadline(network_state);
		}
	}

	if (network_state->bringup_in_progress && network_supervisor_bringup_expired(network_state) &&
	    (!network_state->wifi_connected || !network_state->ipv4_bound)) {
		network_supervisor_clear_bringup(network_state);
		if (network_state->next_retry_at_ms == 0) {
			network_supervisor_record_failure(
				network_state,
				network_state->wifi_connected ? NETWORK_FAILURE_STAGE_DHCP
						       : NETWORK_FAILURE_STAGE_CONNECT,
				-ETIMEDOUT);
			network_supervisor_arm_retry_deadline(network_state);
		}
	}

	if (!network_state->wifi_ready) {
		network_supervisor_update_connectivity_state(network_state);
		network_supervisor_schedule_retry(network_state,
					 K_MSEC(network_state->retry_interval_ms));
		return;
	}

	if (!network_state->wifi_connected) {
		if (network_state->bringup_in_progress) {
			network_supervisor_update_connectivity_state(network_state);
			network_supervisor_schedule_retry(network_state,
					 network_supervisor_bringup_delay(network_state));
			return;
		}

		if (network_supervisor_retry_wait_active(network_state)) {
			network_supervisor_update_connectivity_state(network_state);
			network_supervisor_schedule_retry(network_state,
					 network_supervisor_retry_delay(network_state));
			return;
		}

		network_supervisor_clear_retry_deadline(network_state);
		wifi_config = network_supervisor_retry_wifi_config(network_state);
		ret = wifi_lifecycle_connect_once(network_state, &wifi_config);
		if (ret != 0) {
			network_supervisor_record_failure(network_state, NETWORK_FAILURE_STAGE_CONNECT, ret);
			network_supervisor_arm_retry_deadline(network_state);
			network_supervisor_update_connectivity_state(network_state);
			network_supervisor_schedule_retry(network_state,
					 network_supervisor_retry_delay(network_state));
			return;
		}

		network_supervisor_start_bringup(network_state);
		network_supervisor_update_connectivity_state(network_state);
		network_supervisor_schedule_retry(network_state,
					 network_supervisor_bringup_delay(network_state));
		return;
	}

	if (!network_state->ipv4_bound) {
		if (network_state->ipv4_lease_lost && network_state->next_retry_at_ms == 0) {
			network_supervisor_record_failure(network_state, NETWORK_FAILURE_STAGE_DHCP,
						  -ENETDOWN);
			network_supervisor_arm_retry_deadline(network_state);
		}

		if (network_state->bringup_in_progress) {
			network_supervisor_update_connectivity_state(network_state);
			network_supervisor_schedule_retry(network_state,
					 network_supervisor_bringup_delay(network_state));
			return;
		}

		if (network_supervisor_retry_wait_active(network_state)) {
			network_supervisor_update_connectivity_state(network_state);
			network_supervisor_schedule_retry(network_state,
					 network_supervisor_retry_delay(network_state));
			return;
		}

		network_supervisor_clear_retry_deadline(network_state);
		wifi_config = network_supervisor_retry_wifi_config(network_state);
		ret = wifi_lifecycle_connect_once(network_state, &wifi_config);
		if (ret != 0) {
			network_supervisor_record_failure(network_state, NETWORK_FAILURE_STAGE_CONNECT, ret);
			network_supervisor_arm_retry_deadline(network_state);
			network_supervisor_update_connectivity_state(network_state);
			network_supervisor_schedule_retry(network_state,
					 network_supervisor_retry_delay(network_state));
			return;
		}

		network_supervisor_start_bringup(network_state);
		network_supervisor_update_connectivity_state(network_state);
		network_supervisor_schedule_retry(network_state,
					 network_supervisor_bringup_delay(network_state));
		return;
	}

	network_supervisor_clear_retry_deadline(network_state);
	network_supervisor_clear_bringup(network_state);
	ret = reachability_check_host(&network_state->config->reachability);
	network_state->reachability_checked = true;
	network_state->reachability_ok = (ret == 0);
	network_state->last_reachability_status = ret;

	if (ret != 0) {
		network_supervisor_record_failure(network_state, NETWORK_FAILURE_STAGE_REACHABILITY,
					  ret);
		network_supervisor_complete_startup(network_state);
		network_supervisor_update_connectivity_state(network_state);
		network_supervisor_arm_retry_deadline(network_state);
		network_supervisor_schedule_retry(network_state,
					 network_supervisor_retry_delay(network_state));
		return;
	}

	network_supervisor_complete_startup(network_state);
	network_supervisor_update_connectivity_state(network_state);
}

void network_supervisor_init(struct network_runtime_state *network_state, struct net_if *wifi_iface)
{
	if (network_state == NULL) {
		return;
	}

	wifi_lifecycle_init(network_state, wifi_iface);
	k_sem_init(&network_state->startup_outcome_sem, 0, 1);
	k_work_init_delayable(&network_state->supervisor_retry_work,
			     network_supervisor_retry_work_handler);
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

	network_state->config = config;
	network_state->retry_interval_ms = config->wifi.retry_interval_ms;
	network_state->startup_deadline_ms = k_uptime_get() + config->wifi.timeout_ms;
	network_state->startup_settled = false;
	network_state->startup_wait_pending = true;
	network_state->last_failure = (struct network_failure_record){0};
	k_sem_reset(&network_state->startup_outcome_sem);
	network_supervisor_clear_bringup(network_state);
	network_supervisor_clear_retry_deadline(network_state);

	ret = wifi_lifecycle_register_callbacks(network_state);
	if (ret != 0) {
		network_supervisor_record_failure(network_state, NETWORK_FAILURE_STAGE_READY, ret);
		network_supervisor_update_connectivity_state(network_state);
		return ret;
	}

	network_supervisor_update_connectivity_state(network_state);
	network_supervisor_schedule_retry(network_state, K_NO_WAIT);

	ret = k_sem_take(&network_state->startup_outcome_sem, K_MSEC(config->wifi.timeout_ms));
	if (ret == -EAGAIN) {
		network_supervisor_clear_bringup(network_state);
		network_supervisor_record_failure(network_state,
					  network_supervisor_startup_failure_stage(network_state),
					  -ETIMEDOUT);
		network_supervisor_complete_startup(network_state);
		network_supervisor_arm_retry_deadline(network_state);
		network_supervisor_update_connectivity_state(network_state);
		network_supervisor_schedule_retry(network_state,
					 network_supervisor_retry_delay(network_state));
		return 0;
	}

	if (ret != 0) {
		network_state->startup_wait_pending = false;
		return ret;
	}

	network_supervisor_update_connectivity_state(network_state);
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
		.last_reachability_status = network_state->last_reachability_status,
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
