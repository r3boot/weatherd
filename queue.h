#include <stdio.h>
#include <stdlib.h>

struct queue_record;
typedef struct queue_record *queue;
typedef int ElementType;

int         queue_is_empty(queue Q);
int         queue_is_full(queue Q);
queue       queue_create(int MaxElements);
void        queue_dispose(queue Q);
void        queue_make_empty(queue Q);
void        queue_enqueue(ElementType X, queue Q);
ElementType queue_put(queue Q);
void        queue_dequeue(queue Q);
ElementType queue_get(queue Q);

