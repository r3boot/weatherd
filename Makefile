PROG= 	  weatherd
SRCS= 	  weatherd.c logging.c serial.c packet.c thread.c aggregate.c gpio.c
NOMAN=    weatherd

CFLAGS+=-std=c99 -I. -Wall -Werror -g -DHAVE_GPIO
LDFLAGS+=-lpthread

.include <bsd.prog.mk>
