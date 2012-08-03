#include <stdlib.h>

struct s_aggregate {
	int host_id;
	float temperature;
	float humidity;
	float rainfall;
	long pressure;
	float wind_speed;
	long wind_direction;
	float light;
};

typedef struct s_aggregate struct_aggregate;

void reset_aggregates();
int update_aggregates(struct s_packet packet);
struct_aggregate calculate_aggregates();
