#pragma once

#include "app/app_config.h"
#include "network/network_state.h"
#include "persistence/persistence.h"
#include "recovery/recovery.h"

struct app_context {
	struct app_config config;
	struct persistence_store persistence;
	struct persisted_config persisted_config;
	struct network_runtime_state network_state;
	struct recovery_manager recovery;
};
