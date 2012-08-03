#include <stdlib.h>

int redis_connect();
void redis_disconnect();
int setup_redis();
void redis_submit(struct_aggregate value);
