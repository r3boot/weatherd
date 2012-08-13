#include <sys/types.h>
#include <sys/gpio.h>
#include <sys/ioctl.h>

#include <err.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <paths.h>

#include "logging.h"
#include "gpio.h"

char *gpio_dev = "\0";
int gpio_pin = 0;

int devfd = -1;

void setup_gpio(char *dev, int pin) {
	char devn[32];
	if (strncmp(_PATH_DEV, dev, sizeof(_PATH_DEV) - 1)) {
		(void)snprintf(devn, sizeof(devn), "%s%s", _PATH_DEV, dev);
		gpio_dev = devn;
	}

	if ((devfd = open(gpio_dev, O_RDWR)) == -1) {
		err(1, "%s", gpio_dev);
	}

	gpio_pin = pin;
}

void gpio_pin_write(int value) {
	struct gpio_pin_op op;

	bzero(&op, sizeof(op));
	op.gp_pin = gpio_pin;
	op.gp_value = (value == 0 ? GPIO_PIN_LOW : GPIO_PIN_HIGH);

	if (value < 2) {
		if (ioctl(devfd, GPIOPINWRITE, &op) == -1) {
			err(1, "GPIOPINWRITE");
		}
	} else {
		if (ioctl(devfd, GPIOPINTOGGLE, &op) == -1) {
			err(1, "GPIOPINTOGGLE");
		}
	}
}

void gpio_reset() {
	log_debug("resetting weatherstation main unit");
	gpio_pin_write(1);
	usleep(5000);
	gpio_pin_write(0);
}
