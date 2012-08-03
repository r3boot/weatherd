PROG= 	  weatherd
SRCS= 	  weatherd.c logging.c serial.c queue.c packet.c thread.c aggregate.c
NOMAN=    weatherd

CFLAGS+= -std=c99 -I. -I/usr/local/include/hiredis -Wall -Werror
# CFLAGS+=  -std=c99 -I. -Wall
LDFLAGS+= -L/usr/local/lib -lpthread

.include <bsd.prog.mk>
