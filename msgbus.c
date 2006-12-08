/* $Id$ */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/tree.h>

#include <err.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "event.h"
#include "evhttp.h"
#include "http-internal.h"

#include "msgbus.h"
#include "auth.h"
#include "match.h"
#include "mimetype.h"

struct msgbus_ctx {
	char		*address;
	int		 port;
	char		*docroot;
};

struct msgbus_channel;

struct msgbus_sub {
	struct evhttp_connection *evcon;
	struct msgbus_channel	 *channel;
	char			 *sender;
	char			 *type;
	struct event		  ev;
	TAILQ_ENTRY(msgbus_sub)	  next;
};
TAILQ_HEAD(msgbus_subs, msgbus_sub);

struct msgbus_channel {
	char			*name;
	struct msgbus_subs	 subs;
	SPLAY_ENTRY(msgbus_channel) next;
};
SPLAY_HEAD(msgbus_channel_tree, msgbus_channel) msgbus_channels = \
	SPLAY_INITIALIZER(&msgbus_channels);

static int
_channel_cmp(struct msgbus_channel *a, struct msgbus_channel *b)
{
	return (strcmp(a->name, b->name));
}
SPLAY_PROTOTYPE(msgbus_channel_tree, msgbus_channel, next, _channel_cmp);
SPLAY_GENERATE(msgbus_channel_tree, msgbus_channel, next, _channel_cmp);

static void
__debug_write(struct evhttp_connection *evcon, void *arg)
{
	//printf("done write\n");
}

static void
msgbus_deliver(const char *channel, const char *sender, const char *type,
    void *buf, int len)
{
	struct msgbus_channel *chan, find = { .name = (char *)channel };
	struct msgbus_sub *sub;
	struct evbuffer *out;

	if ((chan = SPLAY_FIND(msgbus_channel_tree,
		 &msgbus_channels, &find)) == NULL)
		return;
	
	TAILQ_FOREACH(sub, &chan->subs, next) {
		if ((sub->sender == NULL || sender == NULL ||
			match_pattern_list(sender, sub->sender,
			    strlen(sub->sender), 0) == 1) &&
		    (sub->type == NULL || match_pattern_list(type, sub->type,
			strlen(sub->type), 0) == 0)) {
			out = sub->evcon->output_buffer;
			if (sender != NULL)
				evbuffer_add_printf(out, "From: %s\n", sender);
			evbuffer_add_printf(out, "Content-Type: %s\n"
			    "Content-Length: %d\n\n", type, len);
			evbuffer_add(out, buf, len);
			evbuffer_add_printf(out, "--%s\n", BOUNDARY_MARKER);
			evhttp_write_buffer(sub->evcon, __debug_write, NULL);
		}
	}
}

void
msgbus_sub_close(int fd, short event, void *arg)
{
	struct msgbus_sub *sub = (struct msgbus_sub *)arg;
	
	printf("UNSUB %s:%d %s %s %s\n", sub->evcon->address,
	    sub->evcon->port, sub->channel->name,
	    sub->sender ? sub->sender : "*",
	    sub->type ? sub->type : "*");

	TAILQ_REMOVE(&sub->channel->subs, sub, next);
	
	/* Close channel if we're the last subscriber */
	if (TAILQ_EMPTY(&sub->channel->subs)) {
		SPLAY_REMOVE(msgbus_channel_tree, &msgbus_channels,
		    sub->channel);
		free(sub->channel->name);
		free(sub->channel);
	}
	evhttp_connection_free(sub->evcon);
	free(sub->sender);
	free(sub->type);
	free(sub);
}

void
msgbus_sub_open(struct evhttp_request *req,
    const char *channel, const char *sender, const char *type)
{
	struct msgbus_channel *chan, find = { .name = (char *)channel };
	struct msgbus_sub *sub = calloc(1, sizeof(*sub));

	/* Create channel if we're the first subscriber. */
	if ((chan = SPLAY_FIND(msgbus_channel_tree,
		 &msgbus_channels, &find)) == NULL) {
		chan = calloc(1, sizeof(*chan));
		chan->name = strdup(channel);
		TAILQ_INIT(&chan->subs);
		SPLAY_INSERT(msgbus_channel_tree, &msgbus_channels, chan);
	}
	sub->evcon = req->evcon;
	sub->channel = chan;
	if (sender != NULL) sub->sender = strdup(sender);
	if (type != NULL) sub->type = strdup(type);
	
	event_set(&sub->ev, sub->evcon->fd, EV_READ, msgbus_sub_close, sub);
	event_add(&sub->ev, NULL);
	
	TAILQ_INSERT_TAIL(&chan->subs, sub, next);

	printf("SUB %s:%d %s %s %s\n", req->evcon->address, req->evcon->port,
	    sub->channel->name, sub->sender ? sub->sender : "*",
	    sub->type ? sub->type : "*");
}

const char *
msgbus_path_resolve(const char *docroot, const char *uri)
{
	static char path[MAXPATHLEN];
	struct stat st;
	char tmp[MAXPATHLEN];
	int len;

	/* Build up our real path. */
	if (docroot == NULL || uri == NULL)
		return (NULL);
	if ((len = strlcpy(tmp, docroot, sizeof(tmp))) >= sizeof(tmp))
		return (NULL);
	if (strlcat(tmp, uri, sizeof(tmp)) >= sizeof(tmp))
		return (NULL);
	if (realpath(tmp, path) == NULL)
		return (NULL);

	/* Make sure we're rooted in the docroot */
	if (strncmp(path, docroot, len) != 0)
		return (NULL);
	/* ... and the path exists */
	if (stat(path, &st) != 0)
		return (NULL);
	/* ... and is the directory index, if a directory */
	if (S_ISDIR(st.st_mode)) {
		if (strlcat(path, "/index.html", sizeof(path)) >= sizeof(path)
		    || stat(path, &st) != 0)
			return (NULL);
	}
	/* ... and is a regular file */
	if (!S_ISREG(st.st_mode))
		return (NULL);
		
	return (path);
}

void
msgbus_bus_handler(struct msgbus_ctx *ctx, struct evhttp_request *req,
    const char *channel)
{
	struct evkeyvalq params;
	
	switch (req->type) {
	case EVHTTP_REQ_GET:
	{
		evhttp_parse_query(req->uri, &params);
		
		msgbus_sub_open(req, channel,
		    evhttp_find_header(&params, "sender"),
		    evhttp_find_header(&params, "type"));
		
		evhttp_response_code(req, HTTP_OK, "OK");
		evhttp_add_header(req->output_headers, "Content-Type",
		    "multipart/x-mixed-replace;boundary=" BOUNDARY_MARKER);
		evhttp_make_header(req->evcon, req);
		evbuffer_add_printf(req->evcon->output_buffer, "--%s\n",
		    BOUNDARY_MARKER);
		evhttp_write_buffer(req->evcon, __debug_write, NULL);
		break;
	}
	case EVHTTP_REQ_POST:
	{
		const char *sender, *type;
		
		evhttp_parse_query(req->uri, &params);
		sender = auth_parse(evhttp_find_header(req->input_headers,
					"Authorization"));
		type = evhttp_find_header(req->input_headers, "Content-Type");

		printf("PUB %s:%d %s %s %s (%ld)\n", req->evcon->address,
		    req->evcon->port, channel, sender ? sender : "*",
		    type ? type : "*",
		    (long)EVBUFFER_LENGTH(req->input_buffer));
		
		msgbus_deliver(channel, sender, type,
		    EVBUFFER_DATA(req->input_buffer),
		    EVBUFFER_LENGTH(req->input_buffer));
		evhttp_send_reply(req, HTTP_NOCONTENT, "OK", NULL);
		break;
	}
	default:
		evhttp_send_reply(req, 501, "Not Implemented", NULL);
		break;
	}
}

void
msgbus_doc_handler(struct msgbus_ctx *ctx, struct evhttp_request *req)
{
	struct evbuffer *buf;
	const char *path;
	int fd;

	buf = evbuffer_new();
	
	if (req->type == EVHTTP_REQ_GET) {
		path = msgbus_path_resolve(ctx->docroot, req->uri);
		if (path != NULL && (fd = open(path, O_RDONLY, 0)) != -1) {
			char size[12];
			long len;
			evbuffer_read(buf, fd, -1);
			len = EVBUFFER_LENGTH(buf);
			evhttp_add_header(req->output_headers,
			    "Content-Type", mimetype_guess(path));
			snprintf(size, sizeof(size), "%ld", len);
			evhttp_add_header(req->output_headers,
			    "Content-Length", size);
			evhttp_send_reply(req, HTTP_OK, "OK", buf);
			printf("DOC %s:%d %s (%ld)\n", req->evcon->address,
			    req->evcon->port, req->uri, len);
		} else {
			evbuffer_add_printf(buf, "<h1>Not Found</h1>");
			evhttp_send_reply(req, HTTP_NOTFOUND,
			    "Not Found", buf);
		}
	} else {
		evhttp_add_header(req->output_headers, "Allow", "GET");
		evhttp_send_reply(req, 405, "Method Not Allowed", NULL);
	}
	evbuffer_free(buf);
}

void
msgbus_req_handler(struct evhttp_request *req, void *arg)
{
	struct msgbus_ctx *ctx = arg;
	
	if (req != NULL) {
		if (strncmp(req->uri, "/msgbus/", 8) == 0) {
			char *channel = strdup(req->uri + 8);
			strtok(channel, "?");
			msgbus_bus_handler(ctx, req, channel);
			free(channel);
		} else if (ctx->docroot != NULL) {
			msgbus_doc_handler(ctx, req);
		}
	}
}

static void
usage(void)
{
	fprintf(stderr, "usage: msgbus [OPTIONS]\n\n"
	    "Options:\n"
	    "  -d DIRECTORY  document root        (default: none)\n"
	    "  -p PORT       port to listen on    (default: 8080)\n"
	    "  -s ADDRESS    address to listen on (default: any)\n"
	    );
	exit(1);
}

int
main(int argc, char **argv)
{
	struct msgbus_ctx ctx[1];
	struct evhttp *httpd;
	struct stat st;
	char path[MAXPATHLEN];
	int c;

	memset(ctx, 0, sizeof(*ctx));
	ctx->address = "0.0.0.0";
	ctx->port = 8080;
	
	while ((c = getopt(argc, argv, "d:p:s:h?")) != -1) {
		switch (c) {
		case 'd':
			ctx->docroot = realpath(optarg, path);
			break;
		case 'p':
			ctx->port = atoi(optarg);
			break;
		case 's':
			ctx->address = optarg;
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
	
	if (ctx->docroot != NULL) {
		if (stat(ctx->docroot, &st) < 0 || !S_ISDIR(st.st_mode)) {
			fprintf(stderr, "invalid document root: %s\n",
			    ctx->docroot);
			exit(1);
		}
		fprintf(stderr, "listening on %s:%d, serving %s\n",
		    ctx->address, ctx->port, ctx->docroot);
	} else
		fprintf(stderr, "listening on %s:%d\n",
		    ctx->address, ctx->port);
	
	event_init();
	httpd = evhttp_start(ctx->address, ctx->port);
	evhttp_set_gencb(httpd, msgbus_req_handler, ctx);
	event_dispatch();
	
	/* NOTREACHED */
	evhttp_free(httpd);
	
	exit(1);
}
