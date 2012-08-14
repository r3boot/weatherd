PROG= 	  weatherd
SRCS= 	  weatherd.c logging.c config.c serial.c packet.c thread.c aggregate.c gpio.c datalogger.c graphite.c
NOMAN=    weatherd

CFLAGS+=-std=c99 -I. -Wall -Werror -g -DHAVE_GPIO
LDFLAGS+=-lpthread

.include <bsd.prog.mk>
