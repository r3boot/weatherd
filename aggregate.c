#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>

#include "logging.h"
#include "packet.h"
#include "aggregate.h"

struct s_aggregate *A = NULL;

int num_samples = 0;

void reset_aggregates() {
	A->host_id = 0;
	A->temperature = 0.0;
	A->humidity = 0.0;
	A->rainfall = 0.0;
	A->pressure = 0;
	A->wind_speed = 0.0;
	A->wind_direction = 0;
	A->light = 0.0;

	num_samples = 0;
}

int update_aggregates(struct s_packet *packet) {
	num_samples += 1;
	return 0;
}

struct s_aggregate *calculate_aggregates() {
	struct s_aggregate *values = NULL;
	if (!(values = (struct s_aggregate *)malloc(sizeof(struct s_aggregate)))) {
		log_debug("calculate_aggregates: malloc failed");
		return NULL;
	}
	values->host_id = 0;
	values->temperature = 0.0;
	values->humidity = 0.0;
	values->rainfall = 0.0;
	values->pressure = 0;
	values->wind_speed = 0.0;
	values->wind_direction = 0;
	values->light = 0.0;

	values->host_id = A->host_id;

	if (num_samples > 0) {
		values->pressure = (A->pressure / num_samples);
		values->wind_direction = (A->wind_direction / num_samples);
		values->temperature = (A->temperature / num_samples);
		values->humidity = (A->humidity / num_samples);
		values->rainfall = A->rainfall;
		values->light = (A->light / num_samples);
	}

	log_debug("AVERAGES");
	printf("host_id:        %d\n", values->host_id);
	printf("temperature:    %.02f C\n", values->temperature);
	printf("pressure:       %d Pa\n", (unsigned int)values->pressure);
	printf("humidity:       %.02f%%\n", values->humidity);
	printf("light           %.02f lux\n", values->light);
	printf("wind speed:     %.02f M/s\n", values->wind_speed);
	printf("wind direction: %d\n", (unsigned int)values->wind_direction);
	printf("rainfall:       %.02f mm/s\n\n", values->rainfall);

	reset_aggregates();
	return values;
}

void setup_aggregates(int num_samples) {
	if (!(A = (struct s_aggregate *)malloc(sizeof(struct s_aggregate)))) {
		printf("init_aggregates: malloc failed\n");
	}

}
