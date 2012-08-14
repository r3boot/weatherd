#include <time.h>

struct s_datalog_entry {
	char line[80];
	time_t timestamp;
};

int setup_datalogger(char *dir);
int datalogger_write(struct s_datalog_entry *entry);
