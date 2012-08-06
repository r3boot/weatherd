PROG= 	  weatherd
SRCS= 	  weatherd.c logging.c serial.c queue.c packet.c thread.c aggregate.c
NOMAN=    weatherd

CFLAGS+= -std=c99 -I. -Wall -Werror
# CFLAGS+=  -std=c99 -I. -Wall
LDFLAGS+= -lpthread

.include <bsd.prog.mk>
