#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <string.h>

#include "logging.h"
#include "serial.h"
#include "packet.h"
#include "aggregate.h"
#include "redis.h"
#include "queue.h"

#define SERIAL_BUFFER_MAX 1
#define RAW_PACKET_SIZE 80
#define QUEUE_SIZE 1048576
#define RESET_STATS_TIME 86400
#define AGGREGATE_TIME 60

pthread_mutex_t Qp_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t aggregate_mutex = PTHREAD_MUTEX_INITIALIZER;

queue Qp; 	// queue between serial and packet

void *serial_thread(void *queue_ptr) {

	char in[SERIAL_BUFFER_MAX + 1];
	char raw_packet[RAW_PACKET_SIZE];

	int size = 0;
	int j = 0;

	int msg_seen = -1;

	log_debug("starting serial thread");
	while (1) {
		size = serial_read(in, 1, 1000);
		if (size > 0) {

			switch (*in) {
				case '#':
					// printf("[start]: raw_packet: %s; in: %s; j: %d\n", raw_packet, in, j);
					raw_packet[0] = in[0];
					j += 1;

					// TODO: WHY HERE???
					if (msg_seen == 0) {
						printf("%s\n", raw_packet);
						msg_seen = -1;
					}
					// printf("[start]: raw_packet: %s; in: %s; j: %d\n", raw_packet, in, j);
					break;
				case '$':
					raw_packet[j] = in[0];
					raw_packet[j+1] = '\0';
					// printf("[end]:   raw_packet: %s; in: %s; j: %d\n", raw_packet, in, j);
					msg_seen = 0;
					j = 0;
					in[0] = '\0';
					raw_packet[0] = '\0';
					break;
				default:
					if (j > 0) {
						raw_packet[j] = in[0];
						j += 1;
						// printf("[infli]: raw_packet: %s; in: %s; j: %d\n", raw_packet, in, j);
					}
					break;
			}


			// pthread_mutex_lock(&Qp_mutex);
			// queue_enqueue(in[0], Qp);
			// pthread_mutex_unlock(&Qp_mutex);
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
