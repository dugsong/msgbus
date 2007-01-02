
CFLAGS=		-g -Wall -O2 -I./libevent -I./libevent/compat

event_LDADD=	-L./libevent/.libs -levent -levhttp -lresolv -lssl -lcrypto

LIBS=		evmsg
evmsg_SRCS=	evmsg.c
NOPROFILE=	yes

PROGS=		msgbus test-pub test-sub test-curl

msgbus_SRCS=	match.c mimetype.c msgbus.c
msgbus_LDADD=	${event_LDADD}

test-pub_SRCS=	test-pub.c
test-pub_LDADD=	-L. -levmsg ${event_LDADD}

test-sub_SRCS=	test-sub.c
test-sub_LDADD=	-L. -levmsg ${event_LDADD}

test-curl_SRCS=	test-curl.c
test-curl_CFLAGS=-I/usr/include/curl
test-curl_LDADD=-lcurl

NOMAN=		yes

.include "auto.mk"
