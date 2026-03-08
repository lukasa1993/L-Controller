#include <errno.h>

#include <zephyr/logging/log.h>
#include <zephyr/net/net_if.h>

#include "app/bootstrap.h"
#include "network/network_supervisor.h"
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
			.retry_interval_ms = CONFIG_APP_WIFI_RETRY_INTERVAL_MS,
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
	struct network_supervisor_status network_status;
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

	network_supervisor_init(&app_context->network_state, wifi_iface);

	LOG_INF("Booting Wi-Fi bring-up app on %s", app_context->config.board_name);

	ret = network_supervisor_start(&app_context->network_state, &app_context->config);
	if (ret != 0) {
		if (network_supervisor_get_status(&app_context->network_state, &network_status) == 0 &&
		    network_status.last_failure.recorded) {
			LOG_ERR("Network supervisor failed during %s: %d",
				network_supervisor_failure_stage_text(
					network_status.last_failure.failure_stage),
				network_status.last_failure.reason);
		}

		return ret;
	}

	ret = network_supervisor_get_status(&app_context->network_state, &network_status);
	if (ret == 0) {
		LOG_INF("Network supervisor entered %s",
			network_supervisor_connectivity_state_text(
				network_status.connectivity_state));

		if (network_status.connectivity_state != NETWORK_CONNECTIVITY_HEALTHY) {
			LOG_WRN("Continuing boot with background network recovery in %s",
				network_supervisor_connectivity_state_text(
					network_status.connectivity_state));
		}

		if (network_status.last_failure.recorded) {
			LOG_WRN("Most recent network failure during %s: %d",
				network_supervisor_failure_stage_text(
					network_status.last_failure.failure_stage),
				network_status.last_failure.reason);
		}
	}

	LOG_INF("APP_READY");
	return 0;
}
