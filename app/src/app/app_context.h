#pragma once

#include "actions/actions.h"
#include "app/app_config.h"
#include "network/network_state.h"
#include "panel/panel_auth.h"
#include "panel/panel_http.h"
#include "persistence/persistence.h"
#include "recovery/recovery.h"
#include "relay/relay.h"
#include "scheduler/scheduler.h"

struct app_context {
	struct app_config config;
	struct persistence_store persistence;
	struct persisted_config persisted_config;
	struct network_runtime_state network_state;
	struct action_dispatcher actions;
	struct panel_auth_service panel_auth;
	struct panel_http_server panel_http;
	struct recovery_manager recovery;
	struct relay_service relay;
	struct scheduler_service scheduler;
};
