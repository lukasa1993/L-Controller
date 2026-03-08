#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/sys/util.h>

#include <net/wifi_ready.h>

#include "network/wifi_lifecycle.h"

LOG_MODULE_DECLARE(app, CONFIG_LOG_DEFAULT_LEVEL);

#define WIFI_EVENTS (NET_EVENT_WIFI_CONNECT_RESULT | NET_EVENT_WIFI_DISCONNECT_RESULT)
#define IPV4_EVENTS \
	(NET_EVENT_IPV4_DHCP_BOUND | NET_EVENT_IPV4_DHCP_STOP | NET_EVENT_IPV4_ADDR_DEL)

static struct network_runtime_state *wifi_ready_state;

static const char *security_text(enum wifi_security_type security)
{
	switch (security) {
	case WIFI_SECURITY_TYPE_NONE:
		return "none";
	case WIFI_SECURITY_TYPE_PSK:
		return "psk";
	case WIFI_SECURITY_TYPE_PSK_SHA256:
		return "psk-sha256";
	case WIFI_SECURITY_TYPE_SAE:
		return "sae";
	default:
		return "unknown";
	}
}

static k_timeout_t remaining_timeout(int64_t deadline_ms)
{
	int64_t remaining_ms = deadline_ms - k_uptime_get();

	if (remaining_ms <= 0) {
		return K_NO_WAIT;
	}

	return K_MSEC(remaining_ms);
}

static void log_iface_status(const struct network_runtime_state *network_state)
{
	struct wifi_iface_status status = {0};
	int ret;

	ret = net_mgmt(NET_REQUEST_WIFI_IFACE_STATUS, network_state->wifi_iface, &status,
		       sizeof(status));
	if (ret != 0) {
		LOG_WRN("Wi-Fi status request failed: %d", ret);
		return;
	}

	LOG_INF("Wi-Fi state=%d mode=%d band=%d channel=%d security=%s rssi=%d ssid=%.32s",
		status.state,
		status.iface_mode,
		status.band,
		status.channel,
		security_text(status.security),
		status.rssi,
		status.ssid);
}

static void clear_ipv4_state(struct network_runtime_state *network_state)
{
	network_state->ipv4_bound = false;
	network_state->reachability_checked = false;
	network_state->reachability_ok = false;
	network_state->last_reachability_status = -EAGAIN;
	memset(&network_state->leased_ipv4, 0, sizeof(network_state->leased_ipv4));
}

static void handle_wifi_connect_result(struct network_runtime_state *network_state,
				       struct net_mgmt_event_callback *cb)
{
	const struct wifi_status *status = cb->info;

	network_state->connect_status = status->status;
	network_state->wifi_connected = (status->status == 0);

	if (network_state->wifi_connected) {
		network_state->disconnect_seen = false;
		network_state->ipv4_lease_lost = false;
		LOG_INF("Wi-Fi connected");
	} else {
		LOG_ERR("Wi-Fi connection failed: %d", status->status);
	}

	k_sem_give(&network_state->connect_sem);
}

static void handle_wifi_disconnect_result(struct network_runtime_state *network_state,
					  struct net_mgmt_event_callback *cb)
{
	const struct wifi_status *status = cb->info;

	network_state->last_disconnect_status = status->status;
	network_state->connect_status = status->status ? status->status : -ECONNRESET;
	network_state->wifi_connected = false;
	network_state->disconnect_seen = true;
	clear_ipv4_state(network_state);

	LOG_WRN("Wi-Fi disconnected: %d", status->status);
	log_iface_status(network_state);
}

static void handle_ipv4_dhcp_bound(struct network_runtime_state *network_state,
				   struct net_mgmt_event_callback *cb)
{
	const struct net_if_dhcpv4 *dhcpv4 = cb->info;
	char ip_buf[NET_IPV4_ADDR_LEN];

	network_state->leased_ipv4 = dhcpv4->requested_ip;
	network_state->ipv4_bound = true;
	network_state->ipv4_lease_lost = false;
	net_addr_ntop(AF_INET, &network_state->leased_ipv4, ip_buf, sizeof(ip_buf));
	LOG_INF("DHCP IPv4 address: %s", ip_buf);
	k_sem_give(&network_state->ipv4_sem);
}

static void handle_ipv4_dhcp_stop(struct network_runtime_state *network_state)
{
	network_state->ipv4_lease_lost = true;
	clear_ipv4_state(network_state);
	LOG_WRN("DHCP IPv4 lease lost");
}

static void handle_ipv4_addr_del(struct network_runtime_state *network_state)
{
	network_state->ipv4_lease_lost = true;
	clear_ipv4_state(network_state);
	LOG_WRN("IPv4 address removed");
}

static void wifi_event_handler(struct net_mgmt_event_callback *cb, uint64_t mgmt_event,
			       struct net_if *iface)
{
	struct network_runtime_state *network_state =
		CONTAINER_OF(cb, struct network_runtime_state, wifi_event_callback);

	ARG_UNUSED(iface);

	switch (mgmt_event) {
	case NET_EVENT_WIFI_CONNECT_RESULT:
		handle_wifi_connect_result(network_state, cb);
		break;
	case NET_EVENT_WIFI_DISCONNECT_RESULT:
		handle_wifi_disconnect_result(network_state, cb);
		break;
	default:
		break;
	}
}

static void ipv4_event_handler(struct net_mgmt_event_callback *cb, uint64_t mgmt_event,
			       struct net_if *iface)
{
	struct network_runtime_state *network_state =
		CONTAINER_OF(cb, struct network_runtime_state, ipv4_event_callback);

	ARG_UNUSED(iface);

	switch (mgmt_event) {
	case NET_EVENT_IPV4_DHCP_BOUND:
		handle_ipv4_dhcp_bound(network_state, cb);
		break;
	case NET_EVENT_IPV4_DHCP_STOP:
		handle_ipv4_dhcp_stop(network_state);
		break;
	case NET_EVENT_IPV4_ADDR_DEL:
		handle_ipv4_addr_del(network_state);
		break;
	default:
		break;
	}
}

static void wifi_ready_cb(bool ready)
{
	if (wifi_ready_state == NULL) {
		return;
	}

	wifi_ready_state->wifi_ready = ready;
	LOG_INF("Wi-Fi ready state changed: %s", ready ? "ready" : "not-ready");
	k_sem_give(&wifi_ready_state->wifi_ready_sem);
}

void wifi_lifecycle_init(struct network_runtime_state *network_state, struct net_if *wifi_iface)
{
	*network_state = (struct network_runtime_state){
		.wifi_iface = wifi_iface,
		.connect_status = -EAGAIN,
		.last_reachability_status = -EAGAIN,
	};

	k_sem_init(&network_state->wifi_ready_sem, 0, 1);
	k_sem_init(&network_state->connect_sem, 0, 1);
	k_sem_init(&network_state->ipv4_sem, 0, 1);
}

int wifi_lifecycle_security_from_text(const char *security_text_value,
			      enum wifi_security_type *security)
{
	if (strcasecmp(security_text_value, "none") == 0) {
		*security = WIFI_SECURITY_TYPE_NONE;
		return 0;
	}

	if (strcasecmp(security_text_value, "psk") == 0) {
		*security = WIFI_SECURITY_TYPE_PSK;
		return 0;
	}

	if (strcasecmp(security_text_value, "psk-sha256") == 0) {
		*security = WIFI_SECURITY_TYPE_PSK_SHA256;
		return 0;
	}

	if (strcasecmp(security_text_value, "sae") == 0) {
		*security = WIFI_SECURITY_TYPE_SAE;
		return 0;
	}

	return -EINVAL;
}

int wifi_lifecycle_register_callbacks(struct network_runtime_state *network_state)
{
	wifi_ready_callback_t wifi_ready_callback = {
		.wifi_ready_cb = wifi_ready_cb,
	};

	if (network_state == NULL || network_state->wifi_iface == NULL) {
		return -ENODEV;
	}

	wifi_ready_state = network_state;

	net_mgmt_init_event_callback(&network_state->wifi_event_callback, wifi_event_handler,
				     WIFI_EVENTS);
	net_mgmt_add_event_callback(&network_state->wifi_event_callback);

	net_mgmt_init_event_callback(&network_state->ipv4_event_callback, ipv4_event_handler,
				     IPV4_EVENTS);
	net_mgmt_add_event_callback(&network_state->ipv4_event_callback);

	return register_wifi_ready_callback(wifi_ready_callback, network_state->wifi_iface);
}

int wifi_lifecycle_wait_for_ready(struct network_runtime_state *network_state)
{
	int ret;

	LOG_INF("Waiting for Wi-Fi to become ready");
	while (!network_state->wifi_ready) {
		ret = k_sem_take(&network_state->wifi_ready_sem, K_FOREVER);
		if (ret != 0) {
			LOG_ERR("Failed while waiting for Wi-Fi ready: %d", ret);
			return ret;
		}
	}

	return 0;
}

int wifi_lifecycle_connect_once(struct network_runtime_state *network_state,
				const struct app_wifi_config *wifi_config)
{
	struct wifi_connect_req_params params = {0};
	int ret;

	if (wifi_config->ssid == NULL || wifi_config->ssid[0] == '\0') {
		LOG_ERR("CONFIG_APP_WIFI_SSID is empty");
		return -EINVAL;
	}

	if (wifi_config->security != WIFI_SECURITY_TYPE_NONE &&
	    (wifi_config->psk == NULL || wifi_config->psk[0] == '\0')) {
		LOG_ERR("CONFIG_APP_WIFI_PSK is empty for security mode %s",
			security_text(wifi_config->security));
		return -EINVAL;
	}

	params.ssid = (const uint8_t *)wifi_config->ssid;
	params.ssid_length = strlen(wifi_config->ssid);
	params.security = wifi_config->security;
	params.channel = WIFI_CHANNEL_ANY;
	params.band = WIFI_FREQ_BAND_UNKNOWN;
	params.mfp = WIFI_MFP_OPTIONAL;
	params.timeout = wifi_config->timeout_ms / 1000;

	if (wifi_config->security == WIFI_SECURITY_TYPE_SAE) {
		params.sae_password = wifi_config->psk;
		params.sae_password_length = strlen(wifi_config->psk);
	} else if (wifi_config->security != WIFI_SECURITY_TYPE_NONE) {
		params.psk = wifi_config->psk;
		params.psk_length = strlen(wifi_config->psk);
	}

	network_state->connect_status = -EINPROGRESS;
	network_state->wifi_connected = false;
	network_state->disconnect_seen = false;
	network_state->ipv4_lease_lost = false;
	clear_ipv4_state(network_state);
	k_sem_reset(&network_state->connect_sem);
	k_sem_reset(&network_state->ipv4_sem);

	ret = net_mgmt(NET_REQUEST_WIFI_DISCONNECT, network_state->wifi_iface, NULL, 0);
	if (ret != 0 && ret != -EALREADY) {
		LOG_DBG("Pre-connect disconnect returned: %d", ret);
	}

	ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, network_state->wifi_iface, &params,
		       sizeof(params));
	if (ret != 0) {
		LOG_ERR("Wi-Fi connect request failed: %d", ret);
		return ret;
	}

	LOG_INF("Connecting to SSID '%s' with %s security", wifi_config->ssid,
		security_text(wifi_config->security));
	return 0;
}

int wifi_lifecycle_wait_for_connection_and_ipv4(struct network_runtime_state *network_state,
					     int32_t timeout_ms)
{
	int ret;
	int64_t deadline_ms = k_uptime_get() + timeout_ms;

	ret = k_sem_take(&network_state->connect_sem, remaining_timeout(deadline_ms));
	if (ret != 0) {
		LOG_ERR("Timed out waiting for Wi-Fi association after %d ms", timeout_ms);
		return -ETIMEDOUT;
	}

	if (!network_state->wifi_connected) {
		LOG_ERR("Wi-Fi did not connect successfully: %d", network_state->connect_status);
		return network_state->connect_status ? network_state->connect_status : -EIO;
	}

	log_iface_status(network_state);

	ret = k_sem_take(&network_state->ipv4_sem, remaining_timeout(deadline_ms));
	if (ret != 0) {
		LOG_ERR("Timed out waiting for DHCP IPv4 lease after %d ms", timeout_ms);
		return -ETIMEDOUT;
	}

	return 0;
}

bool wifi_lifecycle_has_link_loss(const struct network_runtime_state *network_state)
{
	if (network_state == NULL) {
		return false;
	}

	return network_state->disconnect_seen || network_state->ipv4_lease_lost;
}
