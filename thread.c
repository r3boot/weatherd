#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <string.h>
#include <sys/time.h>
#include <sys/queue.h>

#include "logging.h"
#include "serial.h"
#include "packet.h"
#include "aggregate.h"

#ifdef HAVE_GPIO
#include "gpio.h"
#endif

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

// packet -> aggregates
pthread_mutex_t pa_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t pa_cond = PTHREAD_COND_INITIALIZER;

struct pa_entry {
	struct s_packet *packet;
	SIMPLEQ_ENTRY(pa_entry) pa_entries;
};
SIMPLEQ_HEAD(, pa_entry) pa_head = SIMPLEQ_HEAD_INITIALIZER(pa_head);

// aggregates -> datalogger
pthread_mutex_t da_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t da_cond = PTHREAD_COND_INITIALIZER;

struct da_entry {
	struct s_aggregate *values;
	SIMPLEQ_ENTRY(da_entry) da_entries;
};
SIMPLEQ_HEAD(, da_entry) da_head = SIMPLEQ_HEAD_INITIALIZER(da_head);

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
	struct pa_entry *pae = NULL;
	struct s_packet *packet = NULL;

	log_debug("starting packet thread");
	while (1) {
		pthread_mutex_lock(&rp_mutex);
		pthread_cond_wait(&rp_cond, &rp_mutex);

		if (!(SIMPLEQ_EMPTY(&rp_head))) {
			rpe = SIMPLEQ_FIRST(&rp_head);
			SIMPLEQ_REMOVE_HEAD(&rp_head, rp_entries);

			if (!(packet = (struct s_packet *)malloc(sizeof(struct s_packet)))) {
				printf("packet_thread: s_packet malloc failed\n");
				return NULL;
			}

			packet = process_packet(rpe->payload);
			free(rpe);

			// update_aggregates(packet);

			if (!(pae = (struct pa_entry *)malloc(sizeof(struct pa_entry)))) {
				printf("packet_thread: p_entry malloc failed\n");
				return NULL;
			}
			pae->packet = packet;
			free(packet);

			pthread_mutex_lock(&pa_mutex);
			SIMPLEQ_INSERT_TAIL(&pa_head, pae, pa_entries);
			pthread_cond_signal(&pa_cond);
			pthread_mutex_unlock(&pa_mutex);

		}

		pthread_mutex_unlock(&rp_mutex);

	}

	return 0;
}

void *aggregate_thread(void *queue_ptr) {
	struct pa_entry *pae = NULL;
	struct da_entry *dae = NULL;
	struct s_aggregate *aggregates = NULL;

	log_debug("starting aggregate thread");
	while (1) {
		pthread_mutex_lock(&pa_mutex);
		pthread_cond_wait(&pa_cond, &pa_mutex);

		pae = SIMPLEQ_FIRST(&pa_head);
		SIMPLEQ_REMOVE_HEAD(&pa_head, pa_entries);
		pthread_mutex_unlock(&pa_mutex);

		update_aggregates(pae->packet);
		free(pae);

		if (has_aggregates()) {
			log_debug("got aggregate");
			aggregates = get_aggregates();

			if (!(dae = (struct da_entry *)malloc(sizeof(struct da_entry)))) {
				log_debug("aggregate_thread: dae: malloc failed");
				return NULL;
			}

			dae->values = aggregates;

			pthread_mutex_lock(&da_mutex);
			SIMPLEQ_INSERT_TAIL(&da_head, dae, da_entries);
			pthread_cond_signal(&da_cond);
			pthread_mutex_unlock(&da_mutex);
		}

	}
	return 0;
}

void *datalogger_thread(void *queue_ptr) {
	struct da_entry *dae = NULL;
	log_debug("starting datalogger thread");
	while (1) {
		pthread_mutex_lock(&da_mutex);
		pthread_cond_wait(&da_cond, &da_mutex);
		if (!(SIMPLEQ_EMPTY(&da_head))) {
			dae = SIMPLEQ_FIRST(&da_head);
			SIMPLEQ_REMOVE_HEAD(&da_head, da_entries);
			printf("host_id: %d\n", dae->values->host_id);
			free(dae);
		}
		pthread_mutex_unlock(&da_mutex);
	}
	return 0;
}

void *graphite_thread(void *queue_ptr) {
	log_debug("starting graphite thread");
	return 0;
}

void run_threads() {
	pthread_t t_serial, t_packet, t_aggregate;
	int t_serial_ret, t_packet_ret, t_aggregate_ret;

	// Start main threads
	t_serial_ret = pthread_create(&t_serial, NULL, serial_thread, NULL);
	t_packet_ret = pthread_create(&t_packet, NULL, packet_thread, NULL);
	t_aggregate_ret = pthread_create(&t_aggregate, NULL, aggregate_thread, NULL);

	pthread_join(t_serial, NULL);
	pthread_join(t_packet, NULL);
	pthread_join(t_aggregate, NULL);

}
