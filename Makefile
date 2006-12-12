
CFLAGS=		-g -Wall -O2 -I./libevent -I./libevent/compat

event_LDADD=	-L./libevent -levent -lresolv

LIBS=		evmsg
evmsg_SRCS=	evmsg.c
NOPROFILE=	yes

PROGS=		msgbus test-flood test-sub
msgbus_SRCS=	auth.c match.c mimetype.c msgbus.c
msgbus_LDADD=	${event_LDADD}
test-flood_SRCS= test-flood.c
test-flood_LDADD= -L. -levmsg ${event_LDADD}
test-sub_SRCS=	test-sub.c
test-sub_LDADD=	-L. -levmsg ${event_LDADD}
NOMAN=		yes

.include "auto.mk"
