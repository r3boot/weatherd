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
struct s_aggregate *V = NULL;

int num_packets = 0;
int has_data = 0;

void aggregates_timer_handler(int signum) {
	V = calculate_aggregates();
	reset_aggregates();
	has_data = 1;
}

int has_aggregates() {
	int retval = has_data;
	has_data = 0;
	return retval;
}

struct s_aggregate *get_aggregates() {
	return V;
}

void reset_aggregates() {
	A->host_id = 0;
	A->temperature = 0.0;
	A->humidity = 0.0;
	A->rainfall = 0.0;
	A->pressure = 0.0;
	A->c_pressure = 0.0;
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
	A->c_pressure += packet->c_pressure;
	A->wind_speed += packet->wind_speed;
	A->wind_direction += packet->wind_direction;
	A->light += packet->light;

	num_packets += 1;
	return 0;
}

struct s_aggregate *calculate_aggregates() {
	struct s_aggregate *values = NULL;
	char buf[256] = "\0";

	if (!(values = (struct s_aggregate *)malloc(sizeof(struct s_aggregate)))) {
		log_debug("calculate_aggregates: malloc failed");
		return NULL;
	}

	values->host_id = 0;
	values->temperature = 0.0;
	values->humidity = 0.0;
	values->rainfall = 0.0;
	values->pressure = 0.0;
	values->c_pressure = 0.0;
	values->wind_speed = 0.0;
	values->wind_direction = 0;
	values->light = 0.0;

	values->host_id = A->host_id;

	if (num_packets > 0) {
		values->pressure = (A->pressure / num_packets);
		values->c_pressure = (A->c_pressure / num_packets);
		values->wind_speed = (A->wind_speed / num_packets);
		values->wind_direction = (A->wind_direction / num_packets);
		values->temperature = (A->temperature / num_packets);
		values->humidity = (A->humidity / num_packets);
		values->rainfall = (A->rainfall / num_packets);
		values->light = (A->light / num_packets);
	}

	(void)snprintf(buf, sizeof(buf), "average temperature: %.02f C", values->temperature);
	log_debug(buf);

	(void)snprintf(buf, sizeof(buf), "average pressure: %.02f hPa", values->pressure);
	log_debug(buf);

	(void)snprintf(buf, sizeof(buf), "average c_pressure: %.02f hPa", values->c_pressure);
	log_debug(buf);

	(void)snprintf(buf, sizeof(buf), "average humidity: %.02f %%", values->humidity);
	log_debug(buf);

	(void)snprintf(buf, sizeof(buf), "average light: %.02f lux", values->light);
	log_debug(buf);

	(void)snprintf(buf, sizeof(buf), "average wind speed: %.02f M/s", values->wind_speed);
	log_debug(buf);

	(void)snprintf(buf, sizeof(buf), "average wind direction: %d", (unsigned int)values->wind_direction);
	log_debug(buf);

	(void)snprintf(buf, sizeof(buf), "average rainfall: %.02f mm/s", values->rainfall);
	log_debug(buf);

	reset_aggregates();
	return values;
}

void setup_aggregates(int num_samples) {
	if (!(A = (struct s_aggregate *)malloc(sizeof(struct s_aggregate)))) {
		printf("init_aggregates: A: malloc failed\n");
	}

	if (!(V = (struct s_aggregate *)malloc(sizeof(struct s_aggregate)))) {
		printf("init_aggregates: V: malloc failed\n");
	}

	struct sigaction sa;
	struct itimerval timer ;
	memset(&sa, 0, sizeof(sa)) ;

	sa.sa_handler = &aggregates_timer_handler;
	sigaction(SIGALRM, &sa, NULL);

	timer.it_value.tv_sec = num_samples;
	timer.it_value.tv_usec = 0;
	timer.it_interval.tv_sec = num_samples;
	timer.it_interval.tv_usec = 0 ;
	setitimer (ITIMER_REAL, &timer, NULL) ;

}
