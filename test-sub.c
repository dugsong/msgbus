/* $Id$ */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "event.h"
#include "evmsg.h"

void
recv_msg(const char *channel, const char *type, const char *sender,
    struct evbuffer *msg, void *arg)
{
	printf("%s %s %s [%s]\n", channel, type, sender, EVBUFFER_DATA(msg));
}

static void
usage(void)
{
	fprintf(stderr, "usage: test-sub [-c channel] [-n num_subscriptions] "
	    "[host [port]]\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct rlimit rlim = { RLIM_INFINITY, RLIM_INFINITY };
	char *server = NULL, *channel = "flood";
	int i, num = 1, port = 0;

	while ((i = getopt(argc, argv, "c:n:h?")) != -1) {
		switch (i) {
		case 'c':
			channel = optarg;
			break;
		case 'n':
			num = atoi(optarg);
			break;
		default:
			usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc > 0) {
		server = argv[0];
		if (argc > 1)
			port = atoi(argv[1]);
	}
	setrlimit(RLIMIT_NOFILE, &rlim);
	
	putenv("EVENT_SHOW_METHOD=1");
	event_init();

	evmsg_open(server, port);

	for (i = 0; i < num; i++)
		evmsg_subscribe(channel, NULL, NULL, recv_msg, NULL);
	
	event_dispatch();
	
	evmsg_close();
	
	exit(0);
}
