#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/stat.h>

#include "logging.h"
#include "datalogger.h"

char *logdir = "/tmp";

int setup_datalogger(char *dir) {
	struct stat s_dir;

	stat(dir, &s_dir);

	if (!(s_dir.st_mode & S_IFDIR)) {
		printf("%s is not a directory\n", dir);
		return -1;
	}

	logdir = dir;

	return 0;
}

int datalogger_write(struct s_datalog_entry *entry) {
	extern char *__progname;

	char logfile[1024] = "\0";
	char timestamp[9] = "\0";

	time_t tt_now = time(NULL);
	struct tm *tm_now = localtime(&tt_now);
	strftime(timestamp, sizeof(timestamp), "%Y%m%d", tm_now);

	(void)snprintf(logfile, sizeof(logfile), "%s/%s-%s.log", logdir, __progname, timestamp);

	FILE *fd = fopen(logfile, "a");
	if (fd == NULL) {
		log_error("failed to open logfile");
	}

	fwrite(entry->line, 1, sizeof(entry->line), fd);

	fclose(fd);
	log_debug("datalogger: wrote log entry");

	return 0;
}
