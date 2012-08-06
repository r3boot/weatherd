#include <stdlib.h>

#include "queue.h"
#include "logging.h"

#define MIN_QUEUE_SIZE (5)

struct queue_record {
	int capacity;
	int front;
	int rear;
	int size;
	ElementType *Array;
};

int queue_is_empty(queue Q) {
	return Q->size == 0;
}

int queue_is_full(queue Q) {
	return Q->size == Q->capacity;
}

queue queue_create(int max_elements) {
	queue Q;

	if (max_elements < MIN_QUEUE_SIZE) {
		log_error("queue size is too small");
	}

	Q = malloc (sizeof(struct queue_record));
	if (Q == NULL) {
		log_error("unable to allocate memory for queue object");
	}

	Q->Array = malloc( sizeof(ElementType) * max_elements );
	if (Q->Array == NULL) {
		log_error("unable to allocate memory for queue object");
	}

	Q->capacity = max_elements;
	queue_make_empty(Q);

	return Q;
}

void queue_make_empty(queue Q) {
	Q->size = 0;
	Q->front = 1;
	Q->rear = 0;
}

void queue_dispose(queue Q) {
	if (Q != NULL) {
		free(Q->Array);
		free(Q);
	}
}

static int queue_succ(int Value, queue Q) {
	if (++Value == Q->capacity) {
		Value = 0;
	}
	return Value;
}

void queue_enqueue(ElementType X, queue Q) {
	if (queue_is_full(Q)) {
		log_error("queue is full");
	} else {
		Q->size++;
		Q->rear = queue_succ(Q->rear, Q);
		Q->Array[Q->rear] = X;
	}
}

ElementType queue_front(queue Q) {
	if (!queue_is_empty(Q)) {
		return Q->Array[Q->front];
	}
	log_error("queue is empty");

	/* Return value to avoid warnings from the compiler */
	return 0;
}

void queue_dequeue(queue Q) {
	if (queue_is_empty(Q)) {
		log_error("queue is empty");
	} else {
		Q->size--;
		Q->front = queue_succ(Q->front, Q);
	}
}

ElementType queue_get(queue Q) {

	ElementType X = 0;

	if (queue_is_empty(Q)) {
		log_error("queue is empty");
	} else {
		Q->size--;
		X = Q->Array[Q->front];
		Q->front = queue_succ(Q->front, Q);
	}

	return X;
}

