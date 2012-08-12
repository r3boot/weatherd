#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/queue.h>
#include <pthread.h>

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

struct entry {
	char *value;
	SIMPLEQ_ENTRY(entry) entries;
};

SIMPLEQ_HEAD(, entry) head = SIMPLEQ_HEAD_INITIALIZER(head);

void *push_thread(void *ptr);
void *pop_thread(void *ptr);

void *push_thread(void *ptr) {
	struct entry *e = NULL;
	// char *bla = "hello";
	for (int i=0; i<5; i+=1) {
		pthread_mutex_lock(&mutex);
		if (!(e = (struct entry *)malloc(sizeof(struct entry)))) {
			printf("push_thread: malloc failed for %d\n", i);
			return NULL;
		}
		snprintf(e->value, 2, "%d\0", i);
		SIMPLEQ_INSERT_TAIL(&head, e, entries);
		printf("push_thread: %s inserted\n", e->value);
		free(e);
		pthread_mutex_unlock(&mutex);
	}

	return NULL;
}

void *pop_thread(void *ptr) {
	struct entry *e = NULL;
	sleep(5);
	while (1) {
		if (!(SIMPLEQ_EMPTY(&head))) {
			pthread_mutex_lock(&mutex);
			e = SIMPLEQ_FIRST(&head);
			SIMPLEQ_REMOVE_HEAD(&head, entries);
			printf("pop_thread: %s removed\n", e->value);
			pthread_mutex_unlock(&mutex);
		} else {
			sleep(1);
		}
	}
}

int main(int argc, char *argv[]) {
	pthread_t t_push, t_pop;
	int t_push_ret, t_pop_ret;

	t_push_ret = pthread_create(&t_push, NULL, push_thread, NULL);
	t_pop_ret = pthread_create(&t_pop, NULL, pop_thread, NULL);

	pthread_join(t_push, NULL);
	pthread_join(t_pop, NULL);

}
