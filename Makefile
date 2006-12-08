
CFLAGS=	-g -Wall -O2 -I./libevent -I./libevent/compat

LDADD=	-L./libevent -levent -lresolv

PROGS=	msgbus test-flood

msgbus_SRCS=	auth.c match.c mimetype.c msgbus.c

test-flood_SRCS= test-flood.c

NOMAN=	yes

.include "auto.mk"
