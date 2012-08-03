#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>

#include "logging.h"
#include "serial.h"
#include "packet.h"
#include "aggregate.h"
#include "redis.h"
#include "queue.h"

#define SERIAL_BUFFER_MAX 1
#define QUEUE_SIZE 1048576
#define RESET_STATS_TIME 86400
#define AGGREGATE_TIME 60

pthread_mutex_t Qp_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t aggregate_mutex = PTHREAD_MUTEX_INITIALIZER;

queue Qp; 	// queue between serial and packet

void *serial_thread(void *queue_ptr) {

	uint8_t in[SERIAL_BUFFER_MAX];
	int size;

	log_debug("starting serial thread");
	while (1) {
		if ((size = serial_read(in, sizeof(in), 1000)) > 0) {
			if (size > 0) {
				pthread_mutex_lock(&Qp_mutex);
				queue_enqueue(in[0], Qp);
				pthread_mutex_unlock(&Qp_mutex);
			}
		} else if (size < 0) {
			log_debug("error reading port");
		}
	}

	return 0;
}

void *packet_thread(void *queue_ptr) {

	uint8_t value;
	struct s_packet packet;
	time_t t_start, t_cur;

	log_debug("starting packet thread");
	reset_packet_stats();
	t_start = time(NULL);
	while (1) {
		if (!queue_is_empty(Qp)) {
			t_cur = time(NULL);
			if ((t_cur - t_start) > RESET_STATS_TIME) {
				reset_packet_stats();
				t_start = t_cur;
			}
			pthread_mutex_lock(&Qp_mutex);
			value = (uint8_t)queue_get(Qp);
			pthread_mutex_unlock(&Qp_mutex);

			if (packet_process_byte(value) == 0) {
				packet = get_packet();

				pthread_mutex_lock(&aggregate_mutex);
				update_aggregates(packet);
				pthread_mutex_unlock(&aggregate_mutex);
			}
		} else {
			sleep(1);
		}
	}

	return 0;
}

void *aggregate_thread(void *queue_ptr) {

	time_t t_start, t_end;

	struct_aggregate values;

	log_debug("starting aggregate thread");
	t_start = time(NULL);
	while (1) {
		t_end = time(NULL);
		if ((t_end - t_start) > AGGREGATE_TIME) {
			pthread_mutex_lock(&aggregate_mutex);
			values = calculate_aggregates();
			pthread_mutex_unlock(&aggregate_mutex);
			//redis_submit(values);
			print_packet_stats();
			t_start = time(NULL);
		} else {
			sleep(1);
		}

	}
	return 0;
}

void run_threads() {
	pthread_t t_serial, t_packet, t_aggregate;
	int t_serial_ret, t_packet_ret, t_aggregate_ret;

	Qp = queue_create(QUEUE_SIZE);

	t_serial_ret = pthread_create(&t_serial, NULL, serial_thread, NULL);
	t_packet_ret = pthread_create(&t_packet, NULL, packet_thread, NULL);
	t_aggregate_ret = pthread_create(&t_aggregate, NULL, aggregate_thread, NULL);

	pthread_join(t_serial, NULL);
	pthread_join(t_packet, NULL);
	pthread_join(t_aggregate, NULL);
}
