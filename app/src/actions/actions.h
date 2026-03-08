#pragma once

struct app_context;

struct action_dispatcher {
	struct app_context *app_context;
};

int action_dispatcher_init(struct action_dispatcher *dispatcher,
			   struct app_context *app_context);
