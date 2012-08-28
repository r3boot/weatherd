#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <math.h>

#include "packet.h"
#include "logging.h"
#include "serial.h"

struct s_packet packet;

struct s_packet get_packet() {
	return packet;
}

void packet_reset_vars() {
	packet.host_id = 0;
	packet.temperature = 0;
	packet.humidity = 0;
	packet.rainfall = 0;
	packet.wind_speed = 0;
	packet.wind_direction = 0;
	packet.wind_chill = 0.0;
	packet.pressure = 0.0;
	packet.c_pressure = 0.0;
	packet.light = 0;

}

struct s_packet *process_packet(char *payload) {
	struct s_packet *packet = NULL;
	int i = 0;
	long p_checksum, c_checksum = 0;

	if (!(packet = (struct s_packet *)malloc(sizeof(struct s_packet)))) {
		log_debug("process_packet: malloc failed");
		return NULL;
	}

	char *t_payload = strdup(payload);
	t_payload = strtok(t_payload, "#");
	t_payload = strtok(t_payload, "$");

	packet->host_id = atoi(strtok(t_payload, ","));
	c_checksum += packet->host_id;
	for (i=0; i<7; i++) {
		if (i == 0) {
			packet->temperature = atof(strtok(NULL, ","));
			c_checksum += packet->temperature;
		} else if (i == 1) {
			packet->pressure = atol(strtok(NULL, ","));
			c_checksum += packet->pressure;

			// packet->c_pressure = (packet->pressure - (packet->temperature * 1638.13)) + 565;
			// packet->c_pressure = (((packet->pressure * 40) - 69400) / 10000) + 916.43;
			packet->c_pressure = (packet->pressure / 40) + (348.2 - packet->temperature);
		} else if (i == 2) {
			packet->humidity = atof(strtok(NULL, ","));
			c_checksum += packet->humidity;
		} else if (i == 3) {
			packet->light = atof(strtok(NULL, ","));
			c_checksum += packet->light;
		} else if (i == 4) {
			packet->wind_speed = atof(strtok(NULL, ",")) * 3.0;
			c_checksum += (packet->wind_speed / (1000 * 60 * 60));
		} else if (i == 5) {
			packet->wind_direction = atol(strtok(NULL, ","));
			c_checksum += packet->wind_direction;
		} else if (i == 6) {
			packet->rainfall = (atof(strtok(NULL, ","))) * 100;
			c_checksum += packet->rainfall;
		} else if (i == 7) {
			p_checksum = atol(strtok(NULL, ","));
			if (p_checksum != c_checksum) {
				printf("checksum invalid: %d %d\n", (int)p_checksum, (int)c_checksum);
				return NULL;
			}
		}
	}

	if (packet->wind_speed >= 4) { // wind chill is only measured when wind_speed >= 4 KM/h
		packet->wind_chill = 13.12 + 0.6215 * packet->temperature - 11.37 * sqrt(packet->wind_speed) + 0.3965 * 15.28 * sqrt(packet->wind_speed);
	} else {
		packet->wind_chill = packet->temperature;
	}

	return packet;
}
