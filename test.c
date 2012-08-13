#include <sys/types.h>
#include <sys/gpio.h>
#include <sys/ioctl.h>

#include <err.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <paths.h>

char *dev;
int devfd = -1;

void setup_gpio(char *dev);
void gpio_pin_write(int pin, char *pin_name, int value);
void gpio_reset();

void setup_gpio(char *dev) {
	char devn[32];
	if (strncmp(_PATH_DEV, dev, sizeof(_PATH_DEV) - 1)) {
		(void)snprintf(devn, sizeof(devn), "%s%s", _PATH_DEV, dev);
		dev = devn;
	}

	if ((devfd = open(dev, O_RDWR)) == -1) {
		err(1, "%s", dev);
	}
}

void gpio_pin_write(int pin, char *pin_name, int value) {
	struct gpio_pin_op op;

	bzero(&op, sizeof(op));
	if (pin_name != NULL) {
		strlcpy(op.gp_name, pin_name, sizeof(op.gp_name));
	} else {
		op.gp_pin = pin;
	}
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

	if (pin_name) {
		printf("pin %s: state %d -> %d\n", pin_name, op.gp_value,
			(value < 2 ? value : 1 - op.gp_value));
	} else {
		printf("pin %d: state %d -> %d\n", pin, op.gp_value,
			(value < 2 ? value : 1 - op.gp_value));
	}
}

void gpio_reset() {
	gpio_pin_write(0, "ws_reset", 1);
	usleep(5000);
	gpio_pin_write(0, "ws_reset", 0);
}

int main(int argc, char *argv[]) {
	extern char *__progname;

	if (argc < 2) {
		printf("usage: %s <device>\n", __progname);
		return -1;
	}
	dev = argv[1];

	setup_gpio(dev);
	gpio_reset();

}
