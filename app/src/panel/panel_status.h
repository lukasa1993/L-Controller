#pragma once

#include <stddef.h>

struct app_context;

int panel_status_render_json(struct app_context *app_context,
			     char *buffer,
			     size_t buffer_len);
