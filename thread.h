#define SERIAL_BUFFER_MAX 76
#define QUEUE_SIZE 1048576
#define RESET_STATS_TIME 3600
#define AGGREGATE_TIME 60

pthread_mutex_t Qp_mutex;

void *serial_thread(void *queue_ptr);
void *packet_thread(void *queue_ptr);
void run_threads();
