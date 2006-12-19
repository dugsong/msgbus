/* $Id$ */

#include <sys/types.h>
#include <sys/time.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "event.h"
#include "evmsg.h"

struct event	send_ev, stats_ev;
struct timeval	send_tv = { 1, 0 }, stats_tv = { 5, 0 };
int		send_cnt;

void
send_msg(int fd, short event, void *arg)
{
	static int i;
	char *channel = arg;
	struct evbuffer *msg = evbuffer_new();

	evbuffer_add_printf(msg, "<h1>hello world %d</h1>", i++);
	evmsg_publish(channel, "text/html", msg);
	send_cnt++;
	event_add(&send_ev, &send_tv);
}

void
print_stats(int fd, short event, void *arg)
{
	printf("%.1f msgs/s\n", ((float)send_cnt) / stats_tv.tv_sec);
	send_cnt = 0;
	event_add(&stats_ev, &stats_tv);
}

static void
usage(void)
{
	fprintf(stderr, "usage: test-pub [-c channel] [-l address] [-p port] "
	    "[-r msgs_per_sec] [-s]\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	char *server = NULL, *channel = "flood";
	int c, port = 0, use_ssl = 0;

	while ((c = getopt(argc, argv, "c:l:p:r:sh?")) != -1) {
		switch (c) {
		case 'c':
			channel = optarg;
			break;
		case 'l':
			server = optarg;
			break;
		case 'p':
			port = atoi(argv[1]);
			break;
		case 'r':
			send_tv.tv_sec = 0;
			send_tv.tv_usec = 1000000 / atoi(optarg);
			break;
		case 's':
			use_ssl = 1;
			break;
		default:
			usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc > 0)
		usage();
	
	putenv("EVENT_SHOW_METHOD=1");
	event_init();

	evmsg_open(server, port, use_ssl);
	evmsg_set_auth(getenv("USER"), "foobar");
	
	evtimer_set(&send_ev, send_msg, channel);
	event_add(&send_ev, &send_tv);

	evtimer_set(&stats_ev, print_stats, NULL);
	event_add(&stats_ev, &stats_tv);
	
	event_dispatch();
	
	evmsg_close();
	
	exit(0);
}
