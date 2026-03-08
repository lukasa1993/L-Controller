#pragma once

#include <zephyr/net/net_if.h>

#include "app/app_context.h"

int app_bootstrap_init(struct app_context *app_context, struct net_if *wifi_iface);
int app_bootstrap_run(struct app_context *app_context);
