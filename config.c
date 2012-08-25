// Code based on eb/config.h -- http://vanheusden.com/entropybroker/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>

#include "config.h"

char config_yes_no(char *what) {
	if (what[0] == '1' || strcasecmp(what, "yes") == 0 || strcasecmp(what, "on") == 0) {
		return 1;
	}
	return 0;
}

void load_config(const char *config, config_t *pconfig)
{
	char *dummy = strdup(config);
	int linenr = 0;
	FILE *fh = fopen(config, "r");
	if (!fh) {
		printf("error opening configuration file '%s'", config);
		exit(1);
	}

	// set defaults
	pconfig->serial_line = "/dev/tty01";
	pconfig->baudrate = 57600;
#ifdef HAVE_GPIO
	pconfig->gpio_device = "gpio1";
	pconfig->gpio_pin = 16;
#endif
	pconfig->daemonize = -1;
	pconfig->debug = 0;
	pconfig->samples = 60;
	pconfig->user = "nobody";
	pconfig->chroot = "/var/empty";
	pconfig->datalog = "/srv/weatherd";
	pconfig->graphite = NULL;

	for(;;) {
		double parvald;
		int parval;
		char read_buffer[4096], *lf, *par;
		char *cmd = fgets(read_buffer, sizeof(read_buffer), fh), *is;
		if (!cmd) {
			break;
		}
		linenr++;

		if (read_buffer[0] == '#' || read_buffer[0] == ';') {
			continue;
		}

		while(*cmd == ' ') cmd++;

		lf = strchr(read_buffer, '\n');
		if (lf) *lf = 0x00;

		if (strlen(cmd) == 0) {
			continue;
		}

		is = strchr(read_buffer, '=');
		if (!is) {
			printf("invalid line at line %d: '=' missing", linenr);
			exit(1);
		}

		*is = 0x00;
		par = is + 1;

		while(*par == ' ') par++;
		parval = atoi(par);
		parvald = atof(par);

		is--;
		while(*is == ' ') { *is = 0x00 ; is--; }

		if (strcmp(cmd, "serial_line") == 0) {
			pconfig->serial_line = strdup(par);
		} else if (strcmp(cmd, "baudrate") == 0) {
			pconfig->baudrate = parval;
#ifdef HAVE_GPIO
		} else if (strcmp(cmd, "gpio_device") == 0) {
			pconfig->gpio_device = strdup(par);
		} else if (strcmp(cmd, "gpio_pin") == 0 ) {
			pconfig->gpio_pin = parval;
#endif
		} else if (strcmp(cmd, "daemonize") == 0) {
			pconfig->daemonize = config_yes_no(par);
		} else if (strcmp(cmd, "debug") == 0) {
			pconfig->debug = config_yes_no(par);
		} else if (strcmp(cmd, "user") == 0) {
			pconfig->user = strdup(par);
		} else if (strcmp(cmd, "chroot") == 0) {
			pconfig->chroot = strdup(par);
		} else if (strcmp(cmd, "samples") == 0) {
			pconfig->samples = parval;
		} else if (strcmp(cmd, "datalog") == 0) {
			pconfig->datalog = strdup(par);
		} else if (strcmp(cmd, "graphite") == 0) {
			continue;
		} else {
			printf("%s=%s not understood\n", cmd, par);
			exit(1);
		}
	}

	fclose(fh);

	free(dummy);
}
