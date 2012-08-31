#include <stdint.h>

struct s_packet {
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
	int checksum;
};

void packet_reset_vars();
struct s_packet *process_packet(char *payload);
struct s_packet get_packet();
