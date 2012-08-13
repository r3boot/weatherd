#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "logging.h"
#include "packet.h"
#include "aggregate.h"

struct s_aggregate *A = NULL;

int num_packets;

void aggregates_timer_handler(int signum) {
	calculate_aggregates();
	reset_aggregates();
}

void reset_aggregates() {
	A->host_id = 0;
	A->temperature = 0.0;
	A->humidity = 0.0;
	A->rainfall = 0.0;
	A->pressure = 0;
	A->wind_speed = 0.0;
	A->wind_direction = 0;
	A->light = 0.0;

	num_packets = 0;
}

int update_aggregates(struct s_packet *packet) {
	A->host_id = packet->host_id;
	A->temperature += packet->temperature;
	A->humidity += packet->humidity;
	A->rainfall += packet->rainfall;
	A->pressure += packet->pressure;
	A->wind_speed += packet->wind_speed;
	A->wind_direction += packet->wind_direction;
	A->light += packet->light;

	num_packets += 1;
	return 0;
}

struct_aggregate calculate_aggregates() {
	struct_aggregate values;
	values.host_id = 0;
	values.temperature = 0.0;
	values.humidity = 0.0;
	values.rainfall = 0.0;
	values.pressure = 0;
	values.wind_speed = 0.0;
	values.wind_direction = 0;
	values.light = 0.0;

	values.host_id = A->host_id;

	if (num_packets > 0) {
		values.pressure = (A->pressure / num_packets);
		values.wind_direction = (A->wind_direction / num_packets);
		values.temperature = (A->temperature / num_packets);
		values.humidity = (A->humidity / num_packets);
		values.rainfall = (A->rainfall / num_packets);
		values.light = (A->light / num_packets);
	}

	log_debug("AVERAGES");
	printf("host_id:        %d\n", values.host_id);
	printf("temperature:    %.02f C\n", values.temperature);
	printf("pressure:       %d Pa\n", (unsigned int)values.pressure);
	printf("humidity:       %.02f%%\n", values.humidity);
	printf("light           %.02f lux\n", values.light);
	printf("wind speed:     %.02f M/s\n", values.wind_speed);
	printf("wind direction: %d\n", (unsigned int)values.wind_direction);
	printf("rainfall:       %.02f mm/s\n\n", values.rainfall);

	reset_aggregates();
	return values;
}

void setup_aggregates() {
	if (!(A = (struct s_aggregate *)malloc(sizeof(struct s_aggregate)))) {
		printf("init_aggregates: malloc failed\n");
	}

	struct sigaction sa;
	struct itimerval timer ;
	memset ( &sa, 0, sizeof ( sa ) ) ;

	sa.sa_handler = &aggregates_timer_handler;
	sigaction ( SIGALRM, &sa, NULL );

	timer.it_value.tv_sec = 60 ;
	timer.it_value.tv_usec = 0;
	timer.it_interval.tv_sec = 60;
	timer.it_interval.tv_usec = 0 ;
	setitimer ( ITIMER_REAL, &timer, NULL ) ;

}
