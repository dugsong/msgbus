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
recv_msg(const char *type, const char *sender, struct evbuffer *msg, void *arg)
{
	printf("%s %s [%s]\n", type, sender, EVBUFFER_DATA(msg));
}

int
main(int argc, char *argv[])
{
	struct rlimit rlim = { RLIM_INFINITY, RLIM_INFINITY };
	char *channel = (argc > 1) ? argv[1] : "flood";
	int i;

	setrlimit(RLIMIT_NOFILE, &rlim);
	
	putenv("EVENT_SHOW_METHOD=1");
	event_init();

	evmsg_open(NULL, 0);

	for (i = 0; i < 1000; i++)
		evmsg_subscribe(channel, NULL, NULL, recv_msg, NULL);
	
	event_dispatch();
	
	evmsg_close();
	
	exit(0);
}
