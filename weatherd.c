#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <libgen.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include "logging.h"
#include "serial.h"
#include "packet.h"
#include "aggregate.h"
// #include "redis.h"
#include "queue.h"
#include "thread.h"

#define PACKAGE 	"weatherd"
#define VERSION 	"0.1"

int opt_daemonize = -1;
int opt_verbose = -1;
char *opt_line = "/dev/tty01";
int opt_baudrate = 57600;
int opt_parity = 0;
int opt_databits = 8;
int opt_stopbits = 1;
char *opt_user = "nobody";
char *opt_chroot = "/var/empty";

void usage() {
	printf("Usage: weatherd [options]\n");
	printf("\n");
	printf("Options:\n");
	printf(" -h,        --help          show this help message and exit\n");
	printf(" -V,        --version       show the program version and exit\n");
	printf(" -d,        --daemonize     daemonize on startup (%d)\n", opt_daemonize);
	printf(" -v,        --verbose       show verbose output (%d)\n", opt_verbose);
	printf(" -l PORT,   --line=PORT     serial port to connect to (%s)\n", opt_line);
	printf(" -s SPEED,  --speed=SPEED   baudrate to use (%d)\n", opt_baudrate);
	if (opt_parity == PAR_EVEN) {
		printf(" -p PARITY, --parity=PARITY use even/odd/none parity (even)\n");
	} else if (opt_parity == PAR_ODD) {
		printf(" -p PARITY, --parity=PARITY use even/odd/none parity (odd)\n");
	} else if (opt_parity == PAR_NONE) {
		printf(" -p PARITY, --parity=PARITY use even/odd/none parity (none)\n");
	}
	printf(" -D N,      --databits=N    use N databits (%d)\n", opt_databits);
	printf(" -S N,      --stopbits=N    use N stopbits (%d)\n", opt_stopbits);
	printf(" -u USER,   --user=USER     user to run as (%s)\n", opt_user);
	printf(" -c PATH,   --chroot=PATH   chroot to this directory (%s)\n", opt_chroot);
	exit(1);
}

void version() {
	printf("%s %s\n", PACKAGE, VERSION);
	exit(1);
}

int main(int argc, char *argv[]) {
	int ch = 0;
	int newarg = 0;
	char buf[255];

	static struct option longopts[] = {
		{"help", 		no_argument, 		NULL, 	'h'},
		{"version", 	no_argument, 		NULL, 	'V'},
		{"daemonize", 	no_argument, 		NULL, 	'd'},
		{"verbose", 	no_argument, 		NULL, 	'v'},
		{"line", 		required_argument, 	NULL, 	'l'},
		{"speed", 		required_argument, 	NULL, 	's'},
		{"parity", 		required_argument, 	NULL, 	'p'},
		{"databits", 	required_argument,  NULL, 	'D'},
		{"stopbits", 	required_argument, 	NULL, 	'S'},
		{"user", 		required_argument, 	NULL, 	'u'},
		{"chroot", 		required_argument, 	NULL, 	'c'},
		{NULL, 			0, 					NULL, 	0}
	};

	while ((ch = getopt_long(argc, argv, "hVdvl:s:p:D:S:u:c:", longopts, NULL)) != -1)
		switch (ch) {
			case 'd':
				opt_daemonize = 0;
				break;
			case 'v':
				opt_verbose = 0;
				break;
			case 'l':
				opt_line = argv[optind];
				break;
			case 's':
				opt_baudrate = (int)argv[optind];
				break;
			case 'p':
				printf("bla: %s\n\n", argv[optind]);
				if (strcmp(argv[optind], "even"))  {
					opt_parity = PAR_EVEN;
				} else if (strcmp(argv[optind], "odd")) {
					opt_parity = PAR_ODD;
				} else if (strcmp(argv[optind], "none")) {
					opt_parity = PAR_NONE;
				} else {
					printf("%s: Error - parity needs to be one of (even|odd|none)\n\n", PACKAGE);
					usage();
				}
				break;
			case 'D':
				opt_databits = (int)argv[optind];
				break;
			case 'S':
				opt_stopbits = (int)argv[optind];
				break;
			case 'u':
				opt_user = argv[optind];
				break;
			case 'c':
				opt_chroot = argv[optind];
				break;
			case 'h':
				usage();
				break;
			case 'V':
				version();
				break;
			case ':':
				printf("%s: Error - Option `%c' needs a value\n\n", PACKAGE, ch);
				usage();
				break;
			case '?':
				printf("%s: Error - No such option: `%c'\n\n", PACKAGE, ch);
				usage();
				break;
			default:
				usage();
				break;
		}
		newarg = optind;

	argc -= optind;
	argv += optind;

	if (opt_daemonize != -1) {
		if (daemon(1, 1) == -1) {
			printf("%s: - failed to daemonize\n", PACKAGE);
			exit(1);
		}
	}

	setup_logging(opt_verbose, PACKAGE);

	if (setup_serial(opt_line, opt_baudrate, opt_databits, opt_stopbits, opt_parity) == -1) {
		log_error("failed to setup serial\n");
		exit(1);
	}

	if (chroot(opt_chroot) == 0) {
		snprintf(buf, 255, "chrooted to %s", opt_chroot);
		log_debug(buf);
	} else {
		snprintf(buf, 255, "failed to chroot to %s", opt_chroot);
		log_error(buf);
		exit(1);
	}

	if (chdir("/") == -1) {
		log_error("failed to chdir");
		exit(1);
	}

	run_threads();

	serial_close();

}
