#include <errno.h>

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include "relay/relay.h"

LOG_MODULE_DECLARE(app, CONFIG_LOG_DEFAULT_LEVEL);

#if !DT_NODE_HAS_STATUS_OKAY(DT_ALIAS(relay0))
#error "Board must define relay0 alias for the first relay output"
#endif

#define APP_RELAY0_NODE DT_ALIAS(relay0)

#if !DT_NODE_HAS_PROP(APP_RELAY0_NODE, gpios)
#error "Board relay0 alias must define a gpios property"
#endif

static const struct gpio_dt_spec relay0_gpio = GPIO_DT_SPEC_GET(APP_RELAY0_NODE, gpios);

int relay_service_init(struct relay_service *service, struct app_context *app_context)
{
	int ret;

	if (service == NULL || app_context == NULL) {
		return -EINVAL;
	}

	if (!gpio_is_ready_dt(&relay0_gpio)) {
		LOG_ERR("Relay GPIO controller is not ready");
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&relay0_gpio, GPIO_OUTPUT_INACTIVE);
	if (ret != 0) {
		LOG_ERR("Failed to configure relay output: %d", ret);
		return ret;
	}

	service->app_context = app_context;
	return 0;
}
