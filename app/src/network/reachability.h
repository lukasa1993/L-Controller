#pragma once

#include "app/app_config.h"

int reachability_check_host(const struct app_reachability_config *reachability_config);
const char *reachability_result_text(int status);
