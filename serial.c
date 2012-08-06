#include <stdio.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

#include "serial.h"
#include "baudrates.h"
#include "logging.h"
#include "queue.h"

#define PAR_NONE 0
#define PAR_EVEN 1
#define PAR_ODD 2

char *line = "/dev/tty01";
int speed = 57600;
int databits = 8;
int parity = PAR_NONE;
int stopbits = 1;

int serial_fd = -1;

struct termios m_options;

int64_t get_time_us() {
	struct timespec time;
	clock_gettime(CLOCK_MONOTONIC, &time);

	return ((int64_t)time.tv_sec * 1000000LL) + (int64_t)(time.tv_nsec + 500) / 1000LL;
}

int64_t get_time_sec() {
	  return get_time_us() / 1000000.0;
}

void u_sleep(int64_t usecs, volatile bool* stop /*= NULL*/) {
	if (usecs <= 0) {
		return;
	} else if (stop && usecs > 1000000) {
		int64_t now = get_time_us();
		int64_t end = now + usecs;

		while (now < end) {
			struct timespec sleeptime = {};

			if (*stop)
				return;
			else if (end - now >= 1000000)
				sleeptime.tv_sec = 1;
			else
				sleeptime.tv_nsec = ((end - now) % 1000000) * 1000;

				nanosleep(&sleeptime, NULL);
				now = get_time_us();
		}
	} else {
		struct timespec sleeptime;
		sleeptime.tv_sec = usecs / 1000000;
		sleeptime.tv_nsec = (usecs % 1000000) * 1000;

		nanosleep(&sleeptime, NULL);
	}
}

int int_to_rate(int baudrate) {
	for (int i = 0; i < sizeof(baudrates) - 1; i++) {
		if (baudrates[i].rate == baudrate) {
			return baudrates[i].symbol;
		}
	}
	return -1;
}

int serial_set_baudrate(int baudrate) {
	int rate = int_to_rate(baudrate);
	if (rate == -1) {
		char buf[255];
		snprintf(buf, 255, "%i is not a valid baudrate", baudrate);
		log_debug(buf);
		return -1;
	}

	//get the current port attributes
	if (tcgetattr(serial_fd, &m_options) != 0) {
		log_debug("failed to get current port attributes");
		return -1;
	}

	if (cfsetispeed(&m_options, rate) != 0) {
		log_debug("failed to set input baudrate");
		return -1;
	}

	if (cfsetospeed(&m_options, rate) != 0) {
		log_debug("failed to set output baudrate");
		return -1;
	}

	return 0;
}

int setup_serial(char *port_name, int baudrate, int databits, int stopbits, int parity) {

	if (databits < 5 || databits > 8) {
		log_error("databits has to be between 5 and 8");
		return -1;
	}

	if (stopbits != 1 && stopbits != 2) {
		log_error("stopbits has to be 1 or 2");
		return -1;
	}

	if (parity != PAR_NONE && parity != PAR_EVEN && parity != PAR_ODD) {
		log_error("parity has to be none, even or odd");
		return -1;
	}

	serial_fd = open(port_name, O_RDWR | O_NOCTTY | O_NDELAY);
	if (serial_fd == -1) {
		log_error("failed to open serial port");
		return -1;
	}

	fcntl(serial_fd, F_SETFL, 0);

	if (serial_set_baudrate(speed) == -1) {
		log_error("failed to set baudrate");
		return -1;
	}

	m_options.c_cflag |= (CLOCAL | CREAD);
	m_options.c_cflag &= ~HUPCL;

	m_options.c_cflag &= ~CSIZE;
	if (databits == 5) m_options.c_cflag |= CS5;
	if (databits == 6) m_options.c_cflag |= CS6;
	if (databits == 7) m_options.c_cflag |= CS7;
	if (databits == 8) m_options.c_cflag |= CS8;

	m_options.c_cflag &= ~PARENB;
	if (parity == PAR_EVEN || parity == PAR_ODD)
		m_options.c_cflag |= PARENB;
	if (parity == PAR_ODD)
		m_options.c_cflag |= PARODD;

#ifdef CRTSCTS
	m_options.c_cflag &= ~CRTSCTS;
#elif defined(CNEW_RTSCTS)
	m_options.c_cflag &= ~CNEW_RTSCTS;
#endif

	if (stopbits == 1)
		m_options.c_cflag &= ~CSTOPB;
	else
		m_options.c_cflag |= CSTOPB;

	m_options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG | XCASE | ECHOK | ECHONL | ECHOCTL | ECHOPRT | ECHOKE | TOSTOP);

	if (parity == PAR_NONE)
		m_options.c_iflag &= ~INPCK;
	else
		m_options.c_iflag |= INPCK | ISTRIP;

	m_options.c_iflag &= ~(IXON | IXOFF | IXANY | BRKINT | INLCR | IGNCR | ICRNL | IUCLC | IMAXBEL);
	m_options.c_oflag &= ~(OPOST | ONLCR | OCRNL);

	if (tcsetattr(serial_fd, TCSANOW, &m_options) != 0) {
		log_debug("failed to set terminal attributes");
		return -1;
	}

	fcntl(serial_fd, F_SETFL, FNDELAY);

	log_debug("serial initialized");

	return serial_fd;

}

int serial_read(char *data, int len, int64_t usecs /*= -1*/)
{
	fd_set port;
	struct timeval timeout, *tv;

	if (serial_fd == -1) {
		log_debug("serial port is closed");
		return -1;
	}

	int64_t now = get_time_us();
	int64_t target = now + usecs;
	int bytesread = 0;

	FD_ZERO(&port);
	FD_SET(serial_fd, &port);

	while (bytesread < len) {
		if (usecs < 0) {
			tv = NULL;
		} else {
			if (now > target) {
				// log_debug("timeout reached");
				break;
			}

			timeout.tv_sec = (target - now) / 1000000LL;
			timeout.tv_usec = (target - now) % 1000000LL;
			tv = &timeout;
		}

		FD_ZERO(&port);
		FD_SET(serial_fd, &port);

		int returnv = select(serial_fd + 1, &port, NULL, NULL, tv);

		if (returnv == -1) {
			log_debug("select returned -1");
			return -1;
		} else if (returnv == 0) {
			// log_debug("nothing to read\n");
			break; //nothing to read
		}

		returnv = read(serial_fd, data + bytesread, len - bytesread);
		if (returnv == -1) {
			// log_debug("failed to read data from serial port");
			return -1;
		} else if (returnv > 0) {
			// printf("received: %d\n", returnv);
			bytesread += returnv;
			break;
		}

		now = get_time_us();
	}

	if (bytesread > 0) {
		// printf("data: %s\n", data);
		/*
		printf("%s read:", line);

		for (int i = 0; i < bytesread; i++) {
			printf("[%d] 0x%02x", i, (unsigned int)data[i]);
		}

		printf("\n");
		*/
	}

	data[bytesread + 1] = '\0';

	return bytesread;
}

void serial_close() {
	if (serial_fd != -1) {
		close(serial_fd);
		serial_fd = -1;
	}
}


void serial_setdtr(bool high) {
	int status;
	ioctl(serial_fd, TIOCMGET, &status);

	if (high) {
		status |= TIOCM_DTR;
	} else {
		status &= ~TIOCM_DTR;
	}

	ioctl(serial_fd, TIOCMSET, &status);
}

void serial_setrts(bool high) {
	int status;
	ioctl(serial_fd, TIOCMGET, &status);

	if (high)
		status |= TIOCM_RTS;
	else
		status &= ~TIOCM_RTS;

	ioctl(serial_fd, TIOCMSET, &status);
}
