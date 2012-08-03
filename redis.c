#include <hiredis.h>

#include "redis.h"
// #include "packet.h"
#include "aggregate.h"

redisContext *c;
struct s_aggregate A;

int redis_connect() {

	c = redisConnectUnix("/var/run/redis.sock");

	if (c->err) {
		printf("REDIS ERROR: %s\n", c->errstr);
		return -1;
	} else {
		return 0;
	}
}

void redis_disconnect() {
	redisFree(c);
}

int setup_redis() {
	return redis_connect();
}

void redis_submit(struct_aggregate values) {
	A = values;
}
