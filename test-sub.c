/* $Id$ */

#include <sys/types.h>
#include <sys/time.h>

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
	char *channel = (argc > 1) ? argv[1] : "flood";
	
	putenv("EVENT_SHOW_METHOD=1");
	event_init();

	evmsg_open("localhost", 8080);

	evmsg_subscribe(channel, NULL, NULL, recv_msg, NULL);
	
	event_dispatch();
	
	evmsg_close();
	
	exit(0);
}
