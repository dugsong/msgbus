/* $Id$ */

#include <sys/types.h>
#include <sys/time.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "event.h"
#include "evmsg.h"

struct event send_ev, stats_ev;
struct timeval send_tv = { 0, 10000 }, stats_tv = { 5, 0 };
int cnt;

void
send_msg(int fd, short event, void *arg)
{
	static int i;
	char *channel = arg;
	struct evbuffer *msg = evbuffer_new();

	evbuffer_add_printf(msg, "<h1>hello world %d</h1>", i++);
	evmsg_publish(channel, "text/html", msg);
	cnt++;
	event_add(&send_ev, &send_tv);
}

void
print_stats(int fd, short event, void *arg)
{
	printf("%.1f messages/s\n", ((float)cnt) / stats_tv.tv_sec);
	cnt = 0;
	event_add(&stats_ev, &stats_tv);
}

int
main(int argc, char *argv[])
{
	char *channel = (argc > 1) ? argv[1] : "flood";
	
	putenv("EVENT_SHOW_METHOD=1");
	event_init();

	evmsg_open(NULL, 0);
	evmsg_set_auth(getenv("USER"), "foobar");
	
	evtimer_set(&send_ev, send_msg, channel);
	event_add(&send_ev, &send_tv);

	evtimer_set(&stats_ev, print_stats, NULL);
	event_add(&stats_ev, &stats_tv);
	
	event_dispatch();
	
	evmsg_close();
	
	exit(0);
}
