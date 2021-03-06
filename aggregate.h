#include <stdlib.h>

struct s_aggregate {
	int host_id;
	float temperature;
	float humidity;
	float rainfall;
	float pressure;
	float c_pressure;
	float wind_speed;
	long wind_direction;
	float wind_chill;
	float light;
};


void setup_aggregates(int num_samples);
void reset_aggregates();
int has_aggregates();
struct s_aggregate *get_aggregates();
int update_aggregates(struct s_packet *packet);
struct s_aggregate *calculate_aggregates();
