#include <errno.h>

#include <zephyr/logging/log.h>
#include <zephyr/net/net_if.h>

#include "app/bootstrap.h"
#include "network/reachability.h"
#include "network/wifi_lifecycle.h"

LOG_MODULE_DECLARE(app, CONFIG_LOG_DEFAULT_LEVEL);

static int load_app_config(struct app_context *app_context)
{
	int ret;

	app_context->config = (struct app_config){
		.board_name = CONFIG_BOARD,
		.wifi = {
			.ssid = CONFIG_APP_WIFI_SSID,
			.psk = CONFIG_APP_WIFI_PSK,
			.timeout_ms = CONFIG_APP_WIFI_TIMEOUT_MS,
		},
		.reachability = {
			.host = CONFIG_APP_REACHABILITY_HOST,
			.port = 80,
		},
	};

	ret = wifi_lifecycle_security_from_text(CONFIG_APP_WIFI_SECURITY,
					       &app_context->config.wifi.security);
	if (ret != 0) {
		LOG_ERR("Unsupported CONFIG_APP_WIFI_SECURITY value: %s", CONFIG_APP_WIFI_SECURITY);
		return ret;
	}

	return 0;
}

int app_boot(struct app_context *app_context)
{
	struct net_if *wifi_iface;
	int ret;

	if (app_context == NULL) {
		return -EINVAL;
	}

	ret = load_app_config(app_context);
	if (ret != 0) {
		return ret;
	}

	wifi_iface = net_if_get_first_wifi();
	if (wifi_iface == NULL) {
		LOG_ERR("No Wi-Fi interface found");
		return -ENODEV;
	}

	wifi_lifecycle_init(&app_context->network_state, wifi_iface);

	ret = wifi_lifecycle_register_callbacks(&app_context->network_state);
	if (ret != 0) {
		LOG_ERR("Failed to register callbacks: %d", ret);
		return ret;
	}

	LOG_INF("Booting Wi-Fi bring-up app on %s", app_context->config.board_name);

	ret = wifi_lifecycle_wait_for_ready(&app_context->network_state);
	if (ret != 0) {
		return ret;
	}

	ret = wifi_lifecycle_connect_once(&app_context->network_state, &app_context->config.wifi);
	if (ret != 0) {
		return ret;
	}

	ret = wifi_lifecycle_wait_for_connection_and_ipv4(&app_context->network_state,
							 app_context->config.wifi.timeout_ms);
	if (ret != 0) {
		LOG_ERR("Wi-Fi bring-up failed, last disconnect status=%d",
			app_context->network_state.last_disconnect_status);
		return ret;
	}

	ret = reachability_check_host(&app_context->config.reachability);
	if (ret != 0) {
		return ret;
	}

	LOG_INF("APP_READY");
	return 0;
}
