
#include <sys/types.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "event.h"
#include "evhttp.h"
#include "http-internal.h"

struct event time_ev;
struct timeval time_tv = { 0, 10000 };

void
post_done(struct evhttp_request *req, void *arg)
{
	if (req == NULL)
		printf("post_done: request failed - down connection?\n");
	else
		printf("post_done: response code: %d\n", req->response_code);
}

void
send_msg(int fd, short event, void *arg)
{
	struct evhttp_connection *evcon = arg;
	struct evhttp_request *req;
	static int i;
	
	if ((req = evhttp_request_new(post_done, NULL)) == NULL)
		errx(1, "request_new failed");

	evhttp_add_header(req->output_headers,
	    "Authorization", "Basic ZHVnc29uZzpmb29iYXIK");
	evhttp_add_header(req->output_headers, "Content-Type", "text/html");
	evbuffer_add_printf(req->output_buffer, "<h1>hello world %d</h1>",
	    i++);
	if (evhttp_make_request(evcon, req, EVHTTP_REQ_POST,
		"/msgbus/flood") == -1)
		errx(1, "make_request failed");

	event_add(&time_ev, &time_tv);
}

int
main(int argc, char *argv[])
{
	struct evhttp_connection *evcon;
	
	putenv("EVENT_SHOW_METHOD=1");
	
	event_init();
	if ((evcon = evhttp_connection_new("localhost", 8080)) == NULL)
		errx(1, "connection failed");

	//evhttp_connection_set_retries(evcon, -1);
	
	evtimer_set(&time_ev, send_msg, evcon);
	event_add(&time_ev, &time_tv);
	
	event_dispatch();
	
	evhttp_connection_free(evcon);

	exit(0);
}
