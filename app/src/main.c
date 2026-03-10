#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "app/bootstrap.h"

LOG_MODULE_REGISTER(app, CONFIG_LOG_DEFAULT_LEVEL);

static struct app_context app_context;

int main(void)
{
	int ret;

	ret = app_boot(&app_context);
	if (ret != 0) {
		return ret;
	}

	for (;;) {
		k_sleep(K_SECONDS(1));
	}
}
