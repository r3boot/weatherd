#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

char *package;
int verbose = -1;
int pid = -1;

void log_info(char *msg) {
	char now[20];
	time_t tt_now = time(NULL);
	struct tm *tm_now = localtime(&tt_now);
	strftime(now, sizeof(now), "%F %T", tm_now);

	printf("%s - %s[%d]: %s\n", now, package, pid, msg);
}

void log_debug(char *msg) {
	if (verbose != -1) {
		log_info(msg);
	}
}

void log_error(char *msg) {
	log_info(msg);
	exit(1);
}

void setup_logging(int opt_verbose, char *opt_package) {
	verbose = opt_verbose;
	pid = getpid();
	package = opt_package;

	log_debug("logging initialized");
}

