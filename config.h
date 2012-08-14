// Code based on eb/config.h -- http://vanheusden.com/entropybroker/

typedef struct {
	char *serial_line;
	int baudrate;
#ifdef HAVE_GPIO
	char *gpio_device;
	int gpio_pin;
#endif
	int daemonize;
	int debug;
	char *user;
	char *chroot;

	char *datalog;
	struct s_graphite_config *graphite;

} config_t;

void load_config(const char *config, config_t *pconfig);
