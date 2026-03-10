#include <errno.h>

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include "app/app_context.h"
#include "persistence/persistence.h"
#include "recovery/recovery.h"
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

static const char *relay_state_text(bool state)
{
	return state ? "on" : "off";
}

static const char *relay_safety_note_text(const char *safety_note)
{
	return safety_note != NULL ? safety_note : "none";
}

const char *relay_status_source_text(enum relay_status_source source)
{
	switch (source) {
	case RELAY_STATUS_SOURCE_BOOT_POLICY:
		return "boot-policy";
	case RELAY_STATUS_SOURCE_RECOVERY_POLICY:
		return "recovery-policy";
	case RELAY_STATUS_SOURCE_COMMAND:
		return "command";
	case RELAY_STATUS_SOURCE_NONE:
	default:
		return "none";
	}
}

static bool relay_service_started_from_recovery(const struct relay_service *service)
{
	const struct recovery_reset_cause *reset_cause;

	reset_cause = recovery_manager_last_reset_cause(&service->app_context->recovery);
	return reset_cause != NULL && reset_cause->available && reset_cause->recovery_reset;
}

static int relay_service_apply_state(struct relay_service *service,
					 bool actual_state,
					 bool desired_state,
					 enum relay_status_source source,
					 const char *safety_note)
{
	int ret;

	ret = gpio_pin_set_dt(&relay0_gpio, actual_state ? 1 : 0);
	if (ret != 0) {
		LOG_ERR("Failed to drive relay output: %d", ret);
		return ret;
	}

	service->status.actual_state = actual_state;
	service->status.desired_state = desired_state;
	service->status.source = source;
	service->status.safety_note = safety_note;
	return 0;
}

static int relay_service_apply_startup_policy(struct relay_service *service)
{
	const struct persisted_relay *persisted_relay;
	const char *safety_note = NULL;
	enum relay_status_source source = RELAY_STATUS_SOURCE_BOOT_POLICY;
	bool actual_state = false;
	bool desired_state;

	persisted_relay = &service->app_context->persisted_config.relay;
	desired_state = persisted_relay->last_desired_state;

	if (relay_service_started_from_recovery(service)) {
		source = RELAY_STATUS_SOURCE_RECOVERY_POLICY;
		safety_note = "Recovery reset forced relay off until a fresh command.";
		actual_state = false;
	} else {
		switch (persisted_relay->reboot_policy) {
		case PERSISTED_RELAY_REBOOT_POLICY_RESTORE_LAST_DESIRED:
			actual_state = desired_state;
			break;
		case PERSISTED_RELAY_REBOOT_POLICY_SAFE_OFF:
			actual_state = false;
			safety_note = desired_state ?
				"Normal reboot policy forced relay off." : NULL;
			break;
		case PERSISTED_RELAY_REBOOT_POLICY_IGNORE_LAST_DESIRED:
		default:
			actual_state = false;
			safety_note = desired_state ?
				"Normal reboot policy ignored the remembered desired state." : NULL;
			break;
		}
	}

	return relay_service_apply_state(service,
					 actual_state,
					 desired_state,
					 source,
					 safety_note);
}

const struct relay_runtime_status *relay_service_get_status(const struct relay_service *service)
{
	if (service == NULL) {
		return NULL;
	}

	return &service->status;
}

int relay_service_init(struct relay_service *service, struct app_context *app_context)
{
	int ret;

	if (service == NULL || app_context == NULL) {
		return -EINVAL;
	}

	*service = (struct relay_service){
		.app_context = app_context,
		.status = {
			.implemented = true,
			.available = false,
		},
	};

	if (!gpio_is_ready_dt(&relay0_gpio)) {
		LOG_ERR("Relay GPIO controller is not ready");
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&relay0_gpio, GPIO_OUTPUT_INACTIVE);
	if (ret != 0) {
		LOG_ERR("Failed to configure relay output: %d", ret);
		return ret;
	}

	service->status.available = true;
	ret = relay_service_apply_startup_policy(service);
	if (ret != 0) {
		return ret;
	}

	LOG_INF("Relay startup actual=%s desired=%s source=%s reboot=%s note=%s",
		relay_state_text(service->status.actual_state),
		relay_state_text(service->status.desired_state),
		relay_status_source_text(service->status.source),
		persisted_relay_reboot_policy_text(app_context->persisted_config.relay.reboot_policy),
		relay_safety_note_text(service->status.safety_note));

	return 0;
}
