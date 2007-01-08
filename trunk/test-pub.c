/* $Id$ */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "event.h"
#include "evmsg.h"

struct pub_ctx {
	char		*channel;
	char		*type;
	void		*data;
	int		 len;
	
	struct event	 send_ev, stats_ev;
	struct timeval	 send_tv, stats_tv;
	int		 cnt;
} ctx[1];

static void
send_msg(int fd, short event, void *arg)
{
	static int i;
	struct evbuffer *msg = evbuffer_new();

	if (ctx->data != NULL) {
		evbuffer_add(msg, ctx->data, ctx->len);
	} else {
		evbuffer_add_printf(msg, "<h1>hello world %d</h1>", i++);
	}
	evmsg_publish(ctx->channel, ctx->type, msg);
	ctx->cnt++;
	event_add(&ctx->send_ev, &ctx->send_tv);
}

static void
print_stats(int fd, short event, void *arg)
{
	printf("%.1f msgs/s\n", ((float)ctx->cnt) / ctx->stats_tv.tv_sec);
	ctx->cnt = 0;
	event_add(&ctx->stats_ev, &ctx->stats_tv);
}

static void
usage(void)
{
	fprintf(stderr, "usage: test-pub [-c channel] [-f filename] "
	    "[-l address] [-p port] [-r msgs_per_sec] [-s]\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct stat st;
	char *server = NULL;
	int c, fd, port = 0, use_ssl = 0;

	ctx->channel = "flood";
	ctx->type = "text/html";
	ctx->send_tv.tv_sec = 1;
	ctx->stats_tv.tv_sec = 5;
	
	while ((c = getopt(argc, argv, "c:f:l:p:r:sh?")) != -1) {
		switch (c) {
		case 'c':
			ctx->channel = optarg;
			break;
		case 'f':
			if ((fd = open(optarg, O_RDONLY, 0)) != -1) {
				if (fstat(fd, &st) == -1)
					err(1, "fstat");
				ctx->len = st.st_size;
				ctx->data = malloc(st.st_size);
				if (read(fd, ctx->data, ctx->len) != ctx->len)
					err(1, "read");
				close(fd);
			} else
				err(1, "open");
			break;
		case 'l':
			server = optarg;
			break;
		case 'p':
			port = atoi(argv[1]);
			break;
		case 'r':
			ctx->send_tv.tv_sec = 0;
			ctx->send_tv.tv_usec = 1000000 / atoi(optarg);
			break;
		case 's':
			use_ssl = 1;
			break;
		case 't':
			ctx->type = optarg;
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
	
	evtimer_set(&ctx->send_ev, send_msg, NULL);
	event_add(&ctx->send_ev, &ctx->send_tv);

	evtimer_set(&ctx->stats_ev, print_stats, NULL);
	event_add(&ctx->stats_ev, &ctx->stats_tv);
	
	event_dispatch();
	
	evmsg_close();
	
	exit(0);
}
