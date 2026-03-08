#pragma once

#include "app/app_config.h"
#include "network/network_state.h"

struct app_context {
	struct app_config config;
	struct network_runtime_state network_state;
};
