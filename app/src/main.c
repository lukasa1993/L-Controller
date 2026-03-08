#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/wifi_mgmt.h>

#include <net/wifi_ready.h>

LOG_MODULE_REGISTER(app, CONFIG_LOG_DEFAULT_LEVEL);

#define WIFI_EVENTS (NET_EVENT_WIFI_CONNECT_RESULT | NET_EVENT_WIFI_DISCONNECT_RESULT)
#define IPV4_EVENTS (NET_EVENT_IPV4_DHCP_BOUND)

static struct net_if *wifi_iface;
static struct net_mgmt_event_callback wifi_event_cb;
static struct net_mgmt_event_callback ipv4_event_cb;
static K_SEM_DEFINE(wifi_ready_sem, 0, 1);
static K_SEM_DEFINE(connect_sem, 0, 1);
static K_SEM_DEFINE(ipv4_sem, 0, 1);
static bool wifi_ready;
static bool wifi_connected;
static bool ipv4_bound;
static int connect_status = -EAGAIN;
static int last_disconnect_status;
static struct in_addr leased_ipv4;

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

static int app_security_from_kconfig(enum wifi_security_type *security)
{
	if (strcasecmp(CONFIG_APP_WIFI_SECURITY, "none") == 0) {
		*security = WIFI_SECURITY_TYPE_NONE;
		return 0;
	}

	if (strcasecmp(CONFIG_APP_WIFI_SECURITY, "psk") == 0) {
		*security = WIFI_SECURITY_TYPE_PSK;
		return 0;
	}

	if (strcasecmp(CONFIG_APP_WIFI_SECURITY, "psk-sha256") == 0) {
		*security = WIFI_SECURITY_TYPE_PSK_SHA256;
		return 0;
	}

	if (strcasecmp(CONFIG_APP_WIFI_SECURITY, "sae") == 0) {
		*security = WIFI_SECURITY_TYPE_SAE;
		return 0;
	}

	return -EINVAL;
}

static k_timeout_t remaining_timeout(int64_t deadline_ms)
{
	int64_t remaining_ms = deadline_ms - k_uptime_get();

	if (remaining_ms <= 0) {
		return K_NO_WAIT;
	}

	return K_MSEC(remaining_ms);
}

static void log_iface_status(void)
{
	struct wifi_iface_status status = {0};
	int ret;

	ret = net_mgmt(NET_REQUEST_WIFI_IFACE_STATUS, wifi_iface, &status, sizeof(status));
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

static void handle_wifi_connect_result(struct net_mgmt_event_callback *cb)
{
	const struct wifi_status *status = cb->info;

	connect_status = status->status;
	wifi_connected = (status->status == 0);

	if (wifi_connected) {
		LOG_INF("Wi-Fi connected");
	} else {
		LOG_ERR("Wi-Fi connection failed: %d", status->status);
	}

	k_sem_give(&connect_sem);
}

static void handle_wifi_disconnect_result(struct net_mgmt_event_callback *cb)
{
	const struct wifi_status *status = cb->info;

	last_disconnect_status = status->status;
	wifi_connected = false;
	ipv4_bound = false;
	memset(&leased_ipv4, 0, sizeof(leased_ipv4));

	LOG_WRN("Wi-Fi disconnected: %d", status->status);
	log_iface_status();
}

static void handle_ipv4_dhcp_bound(struct net_mgmt_event_callback *cb)
{
	const struct net_if_dhcpv4 *dhcpv4 = cb->info;
	char ip_buf[NET_IPV4_ADDR_LEN];

	leased_ipv4 = dhcpv4->requested_ip;
	ipv4_bound = true;
	net_addr_ntop(AF_INET, &leased_ipv4, ip_buf, sizeof(ip_buf));
	LOG_INF("DHCP IPv4 address: %s", ip_buf);
	k_sem_give(&ipv4_sem);
}

static void wifi_event_handler(struct net_mgmt_event_callback *cb, uint64_t mgmt_event,
			       struct net_if *iface)
{
	ARG_UNUSED(iface);

	switch (mgmt_event) {
	case NET_EVENT_WIFI_CONNECT_RESULT:
		handle_wifi_connect_result(cb);
		break;
	case NET_EVENT_WIFI_DISCONNECT_RESULT:
		handle_wifi_disconnect_result(cb);
		break;
	default:
		break;
	}
}

static void ipv4_event_handler(struct net_mgmt_event_callback *cb, uint64_t mgmt_event,
			       struct net_if *iface)
{
	ARG_UNUSED(iface);

	if (mgmt_event == NET_EVENT_IPV4_DHCP_BOUND) {
		handle_ipv4_dhcp_bound(cb);
	}
}

static void wifi_ready_cb(bool ready)
{
	wifi_ready = ready;
	LOG_INF("Wi-Fi ready state changed: %s", ready ? "ready" : "not-ready");
	k_sem_give(&wifi_ready_sem);
}

static int register_callbacks(void)
{
	wifi_ready_callback_t cb = {
		.wifi_ready_cb = wifi_ready_cb,
	};

	if (!wifi_iface) {
		return -ENODEV;
	}

	net_mgmt_init_event_callback(&wifi_event_cb, wifi_event_handler, WIFI_EVENTS);
	net_mgmt_add_event_callback(&wifi_event_cb);

	net_mgmt_init_event_callback(&ipv4_event_cb, ipv4_event_handler, IPV4_EVENTS);
	net_mgmt_add_event_callback(&ipv4_event_cb);

	return register_wifi_ready_callback(cb, wifi_iface);
}

static int wait_for_wifi_ready_state(void)
{
	int ret;

	LOG_INF("Waiting for Wi-Fi to become ready");
	while (!wifi_ready) {
		ret = k_sem_take(&wifi_ready_sem, K_FOREVER);
		if (ret != 0) {
			LOG_ERR("Failed while waiting for Wi-Fi ready: %d", ret);
			return ret;
		}
	}

	return 0;
}

static int connect_wifi_once(void)
{
	struct wifi_connect_req_params params = {0};
	enum wifi_security_type security;
	int ret;

	if (strlen(CONFIG_APP_WIFI_SSID) == 0U) {
		LOG_ERR("CONFIG_APP_WIFI_SSID is empty");
		return -EINVAL;
	}

	ret = app_security_from_kconfig(&security);
	if (ret != 0) {
		LOG_ERR("Unsupported CONFIG_APP_WIFI_SECURITY value: %s", CONFIG_APP_WIFI_SECURITY);
		return ret;
	}

	if (security != WIFI_SECURITY_TYPE_NONE && strlen(CONFIG_APP_WIFI_PSK) == 0U) {
		LOG_ERR("CONFIG_APP_WIFI_PSK is empty for security mode %s", security_text(security));
		return -EINVAL;
	}

	params.ssid = (const uint8_t *)CONFIG_APP_WIFI_SSID;
	params.ssid_length = strlen(CONFIG_APP_WIFI_SSID);
	params.security = security;
	params.channel = WIFI_CHANNEL_ANY;
	params.band = WIFI_FREQ_BAND_UNKNOWN;
	params.mfp = WIFI_MFP_OPTIONAL;
	params.timeout = CONFIG_APP_WIFI_TIMEOUT_MS / 1000;

	if (security == WIFI_SECURITY_TYPE_SAE) {
		params.sae_password = CONFIG_APP_WIFI_PSK;
		params.sae_password_length = strlen(CONFIG_APP_WIFI_PSK);
	} else if (security != WIFI_SECURITY_TYPE_NONE) {
		params.psk = CONFIG_APP_WIFI_PSK;
		params.psk_length = strlen(CONFIG_APP_WIFI_PSK);
	}

	connect_status = -EINPROGRESS;
	wifi_connected = false;
	ipv4_bound = false;
	k_sem_reset(&connect_sem);
	k_sem_reset(&ipv4_sem);

	ret = net_mgmt(NET_REQUEST_WIFI_DISCONNECT, wifi_iface, NULL, 0);
	if (ret != 0 && ret != -EALREADY) {
		LOG_DBG("Pre-connect disconnect returned: %d", ret);
	}

	ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, wifi_iface, &params, sizeof(params));
	if (ret != 0) {
		LOG_ERR("Wi-Fi connect request failed: %d", ret);
		return ret;
	}

	LOG_INF("Connecting to SSID '%s' with %s security", CONFIG_APP_WIFI_SSID,
		security_text(security));
	return 0;
}

static int wait_for_wifi_connection_and_ipv4(void)
{
	int ret;
	int64_t deadline_ms = k_uptime_get() + CONFIG_APP_WIFI_TIMEOUT_MS;

	ret = k_sem_take(&connect_sem, remaining_timeout(deadline_ms));
	if (ret != 0) {
		LOG_ERR("Timed out waiting for Wi-Fi association after %d ms",
			CONFIG_APP_WIFI_TIMEOUT_MS);
		return -ETIMEDOUT;
	}

	if (!wifi_connected) {
		LOG_ERR("Wi-Fi did not connect successfully: %d", connect_status);
		return connect_status ? connect_status : -EIO;
	}

	log_iface_status();

	ret = k_sem_take(&ipv4_sem, remaining_timeout(deadline_ms));
	if (ret != 0) {
		LOG_ERR("Timed out waiting for DHCP IPv4 lease after %d ms",
			CONFIG_APP_WIFI_TIMEOUT_MS);
		return -ETIMEDOUT;
	}

	return 0;
}

static int run_reachability_check(void)
{
	struct addrinfo hints = {0};
	struct addrinfo *result;
	struct addrinfo *entry;
	int err;
	int sock = -1;

	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	LOG_INF("Running reachability check for %s:80", CONFIG_APP_REACHABILITY_HOST);
	err = getaddrinfo(CONFIG_APP_REACHABILITY_HOST, "80", &hints, &result);
	if (err != 0) {
		LOG_ERR("DNS lookup failed for %s: %d", CONFIG_APP_REACHABILITY_HOST, err);
		return -EHOSTUNREACH;
	}

	err = -ECONNREFUSED;
	for (entry = result; entry != NULL; entry = entry->ai_next) {
		sock = socket(entry->ai_family, entry->ai_socktype, entry->ai_protocol);
		if (sock < 0) {
			err = -errno;
			continue;
		}

		if (connect(sock, entry->ai_addr, entry->ai_addrlen) == 0) {
			err = 0;
			close(sock);
			sock = -1;
			break;
		}

		err = -errno;
		close(sock);
		sock = -1;
	}

	freeaddrinfo(result);

	if (err != 0) {
		LOG_ERR("Reachability check failed: %d", err);
		return err;
	}

	LOG_INF("Reachability check passed");
	return 0;
}

int main(void)
{
	int ret;

	wifi_iface = net_if_get_first_wifi();
	if (!wifi_iface) {
		LOG_ERR("No Wi-Fi interface found");
		return -ENODEV;
	}

	ret = register_callbacks();
	if (ret != 0) {
		LOG_ERR("Failed to register callbacks: %d", ret);
		return ret;
	}

	LOG_INF("Booting Wi-Fi bring-up app on %s", CONFIG_BOARD);

	ret = wait_for_wifi_ready_state();
	if (ret != 0) {
		return ret;
	}

	ret = connect_wifi_once();
	if (ret != 0) {
		return ret;
	}

	ret = wait_for_wifi_connection_and_ipv4();
	if (ret != 0) {
		LOG_ERR("Wi-Fi bring-up failed, last disconnect status=%d", last_disconnect_status);
		return ret;
	}

	ret = run_reachability_check();
	if (ret != 0) {
		return ret;
	}

	LOG_INF("APP_READY");
	for (;;) {
		k_sleep(K_SECONDS(1));
	}
}
