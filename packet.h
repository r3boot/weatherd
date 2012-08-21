#include <stdint.h>

uint8_t MSG_START;
uint8_t MSG_END;

uint8_t TEMPERATURE;
uint8_t PRESSURE;
uint8_t HUMIDITY;
uint8_t RAINFALL;
uint8_t WINDSPEED;
uint8_t WINDDIRECTION;
uint8_t LIGHT;
uint8_t HOSTID;

uint8_t serial_prev;
uint8_t prev;

int in_msg;

struct s_packet {
	int host_id;
	float temperature;
	float humidity;
	float rainfall;
	float pressure;
	float wind_speed;
	long wind_direction;
	float light;
};

struct s_packet_stats {
	unsigned int total_packets;
	unsigned int total_bytes;
	unsigned int valid_bytes;
	unsigned int valid_packets;
	unsigned int invalid_packet_checksum;
	unsigned int oob_bytes;
	unsigned int unknown_bytes;
	unsigned int packet_too_large;
	unsigned int msg_start;
	unsigned int msg_end;
	unsigned int data_bytes;
};

struct s_packet *process_packet(char *payload);
struct s_packet get_packet();
int packet_process_byte(uint8_t byte);
void reset_packet_stats();
void print_packet_stats();
