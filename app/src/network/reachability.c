#include <errno.h>
#include <stdio.h>

#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>

#include "network/reachability.h"

LOG_MODULE_DECLARE(app, CONFIG_LOG_DEFAULT_LEVEL);

int reachability_check_host(const struct app_reachability_config *reachability_config)
{
	struct addrinfo hints = {0};
	struct addrinfo *result;
	struct addrinfo *entry;
	char port_text[6];
	int err;
	int sock = -1;

	err = snprintf(port_text, sizeof(port_text), "%u", reachability_config->port);
	if (err < 0 || err >= sizeof(port_text)) {
		return -EINVAL;
	}

	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	LOG_INF("Running reachability check for %s:%u", reachability_config->host,
		reachability_config->port);
	err = getaddrinfo(reachability_config->host, port_text, &hints, &result);
	if (err != 0) {
		LOG_ERR("DNS lookup failed for %s: %d", reachability_config->host, err);
		return -EHOSTUNREACH;
	}

	err = -ECONNREFUSED;
	for (entry = result; entry != NULL; entry = entry->ai_next) {
		sock = socket(entry->ai_family, entry->ai_socktype, entry->ai_protocol);
		if (sock < 0) {
			err = -errno;
			continue;
		}

		if (connect(sock, entry->ai_addr, entry->ai_addrlen) == 0) {
			err = 0;
			close(sock);
			sock = -1;
			break;
		}

		err = -errno;
		close(sock);
		sock = -1;
	}

	freeaddrinfo(result);

	if (err != 0) {
		LOG_ERR("Reachability check failed: %d", err);
		return err;
	}

	LOG_INF("Reachability check passed");
	return 0;
}
