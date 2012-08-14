#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <err.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>

#include "graphite.h"
#include "logging.h"

struct s_graphite_config *gc = NULL;

int setup_graphite(struct s_graphite_config *config) {

	gc = config;
	return 0;
}

int graphite_write(struct s_graphite_entry *entry) {
	struct addrinfo hints, *res, *res0;
	int error;
	int save_errno;
	int s;
	const char *cause = NULL;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	error = getaddrinfo(gc->host, gc->port, &hints, &res0);
	if (error)
		errx(1, "%s", gai_strerror(error));
	s = -1;
	for (res = res0; res; res = res->ai_next) {
		s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (s == -1) {
			cause = "socket";
			continue;
		}

		if (connect(s, res->ai_addr, res->ai_addrlen) == -1) {
			cause = "connect";
			save_errno = errno;
			close(s);
			errno = save_errno;
			s = -1;
			continue;
		}

		break;
	}
	if (s == -1)
		err(1, "%s", cause);

	freeaddrinfo(res0);

	write(s, &entry->data, sizeof(entry->data));

	close(s);

	log_debug("graphite: submitted values");

	return 0;
}
