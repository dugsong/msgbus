lib_LTLIBRARIES = libevmsg.la

libevmsg_la_SOURCES = evmsg.c
libevmsg_la_LIBADD = @LTLIBOBJS@ -levent_core -levent_extra -lresolv

bin_PROGRAMS = msgbus test-pub test-sub

msgbus_SOURCES = msgbus.c match.c mimetype.c
msgbus_LDADD = libevmsg.la -levent_core -levent_extra -lresolv

test_pub_SOURCES = test-pub.c
test_pub_LDADD = libevmsg.la -levent_core -levent_extra -lresolv

test_sub_SOURCES = test-sub.c
test_sub_LDADD = libevmsg.la -levent_core -levent_extra -lresolv
