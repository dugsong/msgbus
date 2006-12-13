/* $Id$ */

#include <sys/types.h>
#include <sys/time.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "event.h"
#include "evmsg.h"

struct event time_ev;
struct timeval time_tv = { 0, 10000 };

void
send_msg(int fd, short event, void *arg)
{
	static int i;
	char *channel = arg;
	struct evbuffer *msg = evbuffer_new();

	evbuffer_add_printf(msg, "<h1>hello world %d</h1>", i++);
	evmsg_publish(channel, "text/html", msg);
	event_add(&time_ev, &time_tv);
}

int
main(int argc, char *argv[])
{
	char *channel = (argc > 1) ? argv[1] : "flood";
	
	putenv("EVENT_SHOW_METHOD=1");
	event_init();

	evmsg_open("localhost", 8080);
	evmsg_set_auth(getenv("USER"), "foobar");
	
	evtimer_set(&time_ev, send_msg, channel);
	event_add(&time_ev, &time_tv);
	
	event_dispatch();
	
	evmsg_close();
	
	exit(0);
}
