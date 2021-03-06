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
#include "config.h"
#include "serial.h"
#include "packet.h"
#include "thread.h"
#include "aggregate.h"
#include "datalogger.h"
#include "graphite.h"

#ifdef HAVE_GPIO
#include "gpio.h"
#endif

#define VERSION 	"0.9.1"

#define D_DAEMONIZE -1
#define D_VERBOSE -1
#define D_SAMPLES 60
#define D_LINE "/dev/tty01"
#define D_BAUDRATE 57600
#define D_PARITY 0
#define D_DATABITS 8
#define D_STOPBITS 1
#define D_USER "nobody"
#define D_CHROOT "/var/empty"
#define D_DATALOG "/srv/weatherd"
#define D_GRAPHITE NULL
#ifdef HAVE_GPIO
#define D_RESET -1
#define D_GPIODEV "gpio1"
#define D_GPIOPIN 16
#endif

int opt_daemonize = D_DAEMONIZE;
int opt_verbose = D_VERBOSE;;
int opt_samples = D_SAMPLES;
char *opt_line = D_LINE;
int opt_baudrate = D_BAUDRATE;
int opt_parity = D_PARITY;
int opt_databits = D_DATABITS;
int opt_stopbits = D_STOPBITS;
char *opt_user = D_USER;
char *opt_chroot = D_CHROOT;
char *opt_datalog = D_DATALOG;
struct s_graphite_config *opt_graphite = D_GRAPHITE;
#ifdef HAVE_GPIO
int opt_reset = D_RESET;
char *opt_gpiodev = D_GPIODEV;
int opt_gpiopin = D_GPIOPIN;
#endif

extern char *__progname;

void usage() {
	printf("Usage: %s [options]\n", __progname);
	printf("\n");
	printf("Options:\n");
	printf(" -h,        --help          show this help message and exit\n");
	printf(" -V,        --version       show the program version and exit\n");
	printf(" -v,        --verbose       show verbose output (%d)\n", opt_verbose);

	printf("\nServer options:\n");
	printf(" -d,        --daemonize     daemonize on startup (%d)\n", opt_daemonize);
	printf(" -u USER,   --user=USER     user to run as (%s)\n", opt_user);
	printf(" -c PATH,   --chroot=PATH   chroot to this directory (%s)\n", opt_chroot);
	printf(" -a N       --samples=N     aggregate N samples (%d)\n", opt_samples);

	printf("\nSerial options:\n");
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

	printf("\nOutput options:\n");
	printf(" -L PATH,   --datalog=PATH  log aggregates to this directory (%s)\n", opt_datalog);

#ifdef HAVE_GPIO
	printf("\nGPIO options:\n");
	printf(" -r,        --reset         reset the weatherstation main unit (%d)\n", opt_reset);
	printf(" -G,        --gpiodev       gpio device used for resetting (%s)\n", opt_gpiodev);
	printf(" -P,        --gpiopin       gpio pin used for resetting (%d)\n", opt_gpiopin);
#endif
	printf("\n");
	exit(1);
}

void version() {
	printf("%s %s\n", __progname, VERSION);
	exit(1);
}

int main(int argc, char *argv[]) {
	int ch = 0;
	int newarg = 0;
	char *buf = "\0";
	config_t config;

	static struct option longopts[] = {
		{"help", 		no_argument, 		NULL, 	'h'},
		{"version", 	no_argument, 		NULL, 	'V'},
		{"daemonize", 	no_argument, 		NULL, 	'd'},
		{"verbose", 	no_argument, 		NULL, 	'v'},
#ifdef HAVE_GPIO
		{"reset", 		no_argument, 		NULL, 	'r'},
		{"gpiodev", 	required_argument, 	NULL, 	'G'},
		{"gpiopin", 	required_argument, 	NULL, 	'P'},
#endif
		{"samples", 	required_argument, 	NULL, 	'a'},
		{"datalog", 	required_argument, 	NULL, 	'L'},
		{"line", 		required_argument, 	NULL, 	'l'},
		{"speed", 		required_argument, 	NULL, 	's'},
		{"parity", 		required_argument, 	NULL, 	'p'},
		{"databits", 	required_argument,  NULL, 	'D'},
		{"stopbits", 	required_argument, 	NULL, 	'S'},
		{"user", 		required_argument, 	NULL, 	'u'},
		{"chroot", 		required_argument, 	NULL, 	'c'},
		{"graphite", 	required_argument, 	NULL, 	'g'},
		{NULL, 			0, 					NULL, 	0}
	};

#ifdef HAVE_GPIO
	while ((ch = getopt_long(argc, argv, "hVdvra:l:L:s:p:D:S:u:c:g:", longopts, NULL)) != -1)
#else
	while ((ch = getopt_long(argc, argv, "hVdva:l:L:s:p:D:S:u:c:g:", longopts, NULL)) != -1)
#endif
		switch (ch) {
			case 'd':
				opt_daemonize = 0;
				break;
			case 'v':
				opt_verbose = 0;
				break;
#ifdef HAVE_GPIO
			case 'r':
				opt_reset = 0;
				break;
			case 'G':
				opt_gpiodev = optarg;
				break;
			case 'P':
				opt_gpiopin = atoi(optarg);
				break;
#endif
			case 'a':
				opt_samples = atoi(optarg);
				break;
			case 'L':
				opt_datalog = optarg;
				break;
			case 'l':
				opt_line = optarg;
				break;
			case 's':
				opt_baudrate = atoi(optarg);
				break;
			case 'p':
				if (strcmp(optarg, "even"))  {
					opt_parity = PAR_EVEN;
				} else if (strcmp(optarg, "odd")) {
					opt_parity = PAR_ODD;
				} else if (strcmp(optarg, "none")) {
					opt_parity = PAR_NONE;
				} else {
					printf("%s: Error - parity needs to be one of (even|odd|none)\n\n", __progname);
					usage();
				}
				break;
			case 'D':
				opt_databits = atoi(optarg);
				break;
			case 'S':
				opt_stopbits = atoi(optarg);
				break;
			case 'u':
				opt_user = optarg;
				break;
			case 'c':
				opt_chroot = optarg;
				break;
			case 'g':
				if (!(opt_graphite = (struct s_graphite_config *)malloc(sizeof(struct s_graphite_config)))) {
					printf("s_graphite_config malloc failed\n");
					exit(1);
				}
				char *optarg_cp = strdup(optarg);

				buf = strtok(optarg_cp, ":");
				strlcpy(opt_graphite->host, buf, strlen(buf)+1);

				buf = strtok(NULL, ":");
				strlcpy(opt_graphite->port, buf, strlen(buf)+1);

				buf = strtok(NULL, ":");
				strlcpy(opt_graphite->namespace, buf, strlen(buf)+1);

				break;
			case 'h':
				usage();
				break;
			case 'V':
				version();
				break;
			case ':':
				printf("%s: Error - Option `%c' needs a value\n\n", __progname, ch);
				usage();
				break;
			case '?':
				printf("%s: Error - No such option: `%c'\n\n", __progname, ch);
				usage();
				break;
			default:
				usage();
				break;
		}
		newarg = optind;

	argc -= optind;
	argv += optind;

	load_config("weatherd.conf", &config);

	if (strcmp(opt_line, D_LINE) != 0) {
		config.serial_line = strdup(opt_line);
	} else if (opt_baudrate != D_BAUDRATE) {
		config.baudrate = opt_baudrate;
#ifdef HAVE_GPIO
	} else if (strcmp(opt_gpiodev, D_GPIODEV) != 0) {
		config.gpio_device = opt_gpiodev;
	} else if (opt_gpiopin != D_GPIOPIN) {
		config.gpio_pin = opt_gpiopin;
#endif
	} else if (opt_daemonize != D_DAEMONIZE) {
		config.daemonize = opt_daemonize;
	} else if (opt_verbose != D_VERBOSE) {
		config.debug = opt_verbose;
	} else if (strcmp(opt_user, D_USER) != 0) {
		config.user = opt_user;
	} else if (strcmp(opt_chroot, D_CHROOT) != 0) {
		config.chroot = opt_chroot;
	} else if (strcmp(opt_datalog, D_DATALOG) != 0) {
		config.datalog = opt_datalog;
	}

	setup_logging(config.debug, __progname);

#ifdef HAVE_GPIO
	setup_gpio(config.gpio_device, config.gpio_pin);

	if (opt_reset == 0) {
		gpio_reset();
		exit(0);
	}
#endif

	setup_aggregates(config.samples);

	if (setup_datalogger(config.datalog) != 0) {
		exit(1);
	}

	if (opt_graphite != NULL) {
		setup_graphite(opt_graphite);
	}

	/*
	if (config.daemonize != -1) {
		if (daemon(1, 1) == -1) {
			printf("%s: - failed to daemonize\n", __progname);
			exit(1);
		}
	}
	*/

	if (setup_serial(config.serial_line, config.baudrate, opt_databits, opt_stopbits, opt_parity) == -1) {
		log_error("failed to setup serial\n");
		exit(1);
	}

	/*
	if (chroot(config.chroot) == 0) {
		snprintf(buf, 255, "chrooted to %s", config.chroot);
		log_debug(buf);
	} else {
		snprintf(buf, 255, "failed to chroot to %s", config.chroot);
		log_error(buf);
		exit(1);
	}
	*/

	if (chdir("/") == -1) {
		log_error("failed to chdir");
		exit(1);
	}

	run_threads();

	serial_close();

}
