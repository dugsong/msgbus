
CFLAGS=		-g -Wall -O2 -I./libevent -I./libevent/compat

event_LDADD=	-L./libevent -levent -lresolv -lssl -lcrypto

LIBS=		evmsg
evmsg_SRCS=	evmsg.c
NOPROFILE=	yes

PROGS=		msgbus test-pub test-sub
msgbus_SRCS=	match.c mimetype.c msgbus.c
msgbus_LDADD=	${event_LDADD}
test-pub_SRCS=	test-pub.c
test-pub_LDADD=	-L. -levmsg ${event_LDADD}
test-sub_SRCS=	test-sub.c
test-sub_LDADD=	-L. -levmsg ${event_LDADD}
NOMAN=		yes

.include "auto.mk"
