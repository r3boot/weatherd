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

#define SERIAL_BUFFER_MAX 1
#define RAW_PACKET_SIZE 80
#define QUEUE_SIZE 1048576
#define RESET_STATS_TIME 86400
#define AGGREGATE_TIME 60

// serial -> packet
pthread_mutex_t rp_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t rp_cond = PTHREAD_COND_INITIALIZER;

struct rp_entry {
	char *payload;
	SIMPLEQ_ENTRY(rp_entry) rp_entries;
};

SIMPLEQ_HEAD(, rp_entry) rp_head = SIMPLEQ_HEAD_INITIALIZER(rp_head);

// packet -> collectors
pthread_mutex_t p_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t p_cond = PTHREAD_COND_INITIALIZER;

struct p_entry {
	struct s_packet *packet;
	SIMPLEQ_ENTRY(p_entry) p_entries;
};

SIMPLEQ_HEAD(, p_entry) p_head = SIMPLEQ_HEAD_INITIALIZER(p_head);


void *serial_thread(void *queue_ptr) {
	char raw_packet[254] = "\0";
	struct rp_entry *rpe = NULL;

	log_debug("starting serial thread");
	while (1) {

		if (serial_readln(raw_packet) == 0) {
			if (!(rpe = (struct rp_entry *)malloc(sizeof(struct rp_entry)))) {
				log_debug("serial_thread: malloc failed");
				return NULL;
			}

			rpe->payload = raw_packet;
			pthread_mutex_lock(&rp_mutex);
			SIMPLEQ_INSERT_TAIL(&rp_head, rpe, rp_entries);
			pthread_cond_signal(&rp_cond);
			pthread_mutex_unlock(&rp_mutex);
		}

	}

	return 0;
}

void *packet_thread(void *queue_ptr) {
	struct rp_entry *rpe = NULL;
	struct p_entry *p = NULL;
	struct s_packet *packet = NULL;
	time_t t_start, t_cur;

	log_debug("starting packet thread");
	// reset_packet_stats();
	t_start = time(NULL);
	while (1) {
		if (!(SIMPLEQ_EMPTY(&rp_head))) {
			t_cur = time(NULL);
			if ((t_cur - t_start) > RESET_STATS_TIME) {
				// reset_packet_stats();
				t_start = t_cur;
			}

			pthread_mutex_lock(&rp_mutex);
			pthread_cond_wait(&rp_cond, &rp_mutex);

			rpe = SIMPLEQ_FIRST(&rp_head);
			SIMPLEQ_REMOVE_HEAD(&rp_head, rp_entries);

			pthread_mutex_unlock(&rp_mutex);

			if (!(packet = (struct s_packet *)malloc(sizeof(struct s_packet)))) {
				printf("packet_thread: s_packet malloc failed\n");
				return NULL;
			}

			packet = process_packet(rpe->payload);
			free(rpe);

			update_aggregates(packet);

			if (!(p = (struct p_entry *)malloc(sizeof(struct p_entry)))) {
				printf("packet_thread: p_entry malloc failed\n");
				return NULL;
			}
			p->packet = packet;
			free(packet);

			/*
			pthread_mutex_lock(&p_mutex);

			SIMPLEQ_INSERT_TAIL(&p_head, p, p_entries);
			pthread_cond_signal(&p_cond);

			pthread_mutex_unlock(&p_mutex);
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
			pthread_mutex_lock(&p_mutex);
			values = calculate_aggregates();
			pthread_mutex_unlock(&p_mutex);
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
