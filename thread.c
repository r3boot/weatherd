#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <string.h>
#include <sys/queue.h>

#include "logging.h"
#include "serial.h"
#include "packet.h"
#include "aggregate.h"
// #include "queue.h"

#define SERIAL_BUFFER_MAX 1
#define RAW_PACKET_SIZE 80
#define QUEUE_SIZE 1048576
#define RESET_STATS_TIME 86400
#define AGGREGATE_TIME 60

pthread_mutex_t Qp_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t aggregate_mutex = PTHREAD_MUTEX_INITIALIZER;

struct rp_entry {
	char *payload;
	SIMPLEQ_ENTRY(rp_entry) rp_entries;
};

SIMPLEQ_HEAD(, rp_entry) rp_head = SIMPLEQ_HEAD_INITIALIZER(rp_head);

void *serial_thread(void *queue_ptr) {

	char raw_packet[254] = "\0";

	struct rp_entry *rpe = NULL;

	log_debug("starting serial thread");
	while (1) {

		if (serial_readln(raw_packet) == 0) {
			printf("raw_packet: %s\n", raw_packet);

			pthread_mutex_lock(&Qp_mutex);
			if (!(rpe = (struct rp_entry *)malloc(sizeof(struct rp_entry)))) {
				log_debug("serial_thread: malloc failed");
				return NULL;
			} else {
				rpe->payload = raw_packet;
				printf("push: %s\n", raw_packet);
				SIMPLEQ_INSERT_TAIL(&rp_head, rpe, rp_entries);
			}
			pthread_mutex_unlock(&Qp_mutex);
		}

	}

	return 0;
}

void *packet_thread(void *queue_ptr) {

	// char *raw_packet;
	struct rp_entry *rpe = NULL;
	// struct s_packet packet;
	time_t t_start, t_cur;

	log_debug("starting packet thread");
	reset_packet_stats();
	t_start = time(NULL);
	while (1) {
		if (!(SIMPLEQ_EMPTY(&rp_head))) {
			t_cur = time(NULL);
			if ((t_cur - t_start) > RESET_STATS_TIME) {
				reset_packet_stats();
				t_start = t_cur;
			}
			pthread_mutex_lock(&Qp_mutex);
			rpe = SIMPLEQ_FIRST(&rp_head);
			SIMPLEQ_REMOVE_HEAD(&rp_head, rp_entries);
			pthread_mutex_unlock(&Qp_mutex);

			printf("pop: %s\n", rpe->payload);
			free(rpe);

			/*
			if (packet_process_byte(value) == 0) {
				packet = get_packet();

				pthread_mutex_lock(&aggregate_mutex);
				update_aggregates(packet);
				pthread_mutex_unlock(&aggregate_mutex);
			}
			*/
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

	// Qp = queue_create(QUEUE_SIZE);

	t_serial_ret = pthread_create(&t_serial, NULL, serial_thread, NULL);
	t_packet_ret = pthread_create(&t_packet, NULL, packet_thread, NULL);
	t_aggregate_ret = pthread_create(&t_aggregate, NULL, aggregate_thread, NULL);

	pthread_join(t_serial, NULL);
	pthread_join(t_packet, NULL);
	pthread_join(t_aggregate, NULL);

}
