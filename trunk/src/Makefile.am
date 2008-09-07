INCLUDES = @EVENTINC@

lib_LTLIBRARIES = libevmsg.la

libevmsg_la_SOURCES = evmsg.h evmsg.c
libevmsg_la_LIBADD = @LTLIBOBJS@ @EVENTLIB@

bin_PROGRAMS = msgbus test-pub test-sub

msgbus_SOURCES = msgbus.c match.h match.c mimetype.h mimetype.c
msgbus_LDADD = libevmsg.la @EVENTLIB@

test_pub_SOURCES = test-pub.c
test_pub_LDADD = libevmsg.la @EVENTLIB@

test_sub_SOURCES = test-sub.c
test_sub_LDADD = libevmsg.la @EVENTLIB@

EXTRA_DIST = sys/queue.h sys/tree.h
