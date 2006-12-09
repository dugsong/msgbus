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
//#include "http-internal.h"

#include "auth.h"
#include "match.h"
#include "mimetype.h"

#define BOUNDARY_MARKER		"00MsgBus00"

struct msgbus_ctx {
	char		*address;
	int		 port;
	char		*docroot;
};

struct msgbus_channel;

struct msgbus_sub {
	struct evhttp_request	*req;
	struct msgbus_channel	*channel;
	char			*sender;
	char			*type;
	struct event		 ev;
	TAILQ_ENTRY(msgbus_sub)	 next;
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

static const char *
_evhttp_peername(struct evhttp_connection *evcon)
{
	static char buf[128];
	char *address;
	u_short port;

	evhttp_connection_get_peer(evcon, &address, &port);
	snprintf(buf, sizeof(buf), "%s:%d", address, port);
	return (buf);
}

static void
msgbus_deliver(const char *channel, const char *sender, const char *type,
    void *buf, int len)
{
	struct msgbus_channel *chan, find = { .name = (char *)channel };
	struct msgbus_sub *sub;
	
	if ((chan = SPLAY_FIND(msgbus_channel_tree, &msgbus_channels,
		 &find)) == NULL) {
		return;
	}
	TAILQ_FOREACH(sub, &chan->subs, next) {
		if ((sub->sender == NULL || sender == NULL ||
		     match_pattern_list(sender, sub->sender,
			 strlen(sub->sender), 0) == 1) &&
		    (sub->type == NULL || match_pattern_list(type, sub->type,
			strlen(sub->type), 0) == 0)) {
			/*
			 * XXX - can't do this once for the loop using
			 * evbuffer_add_buffer() due to the dirty swap!
			 */
			struct evbuffer *out = evbuffer_new();
			if (sender != NULL)
				evbuffer_add_printf(out, "From: %s\n", sender);
			evbuffer_add_printf(out, "Content-Type: %s\n", type);
			if (sub->req->minor != 1) {
				/* not chunked encoding */
				evbuffer_add_printf(out,
				    "Content-Length: %d\n", len);
			}
			evbuffer_add(out, "\n", 1);
			evbuffer_add(out, buf, len);
			evbuffer_add_printf(out, "--%s\n", BOUNDARY_MARKER);
			evhttp_send_reply_data(sub->req, out);
			evbuffer_free(out);
		}
	}
}

void
msgbus_sub_close(struct evhttp_connection *evcon, void *arg)
{
	struct msgbus_sub *sub = (struct msgbus_sub *)arg;
	struct msgbus_channel *chan = sub->channel;
	
	printf("UNSUB %s %s %s %s\n", _evhttp_peername(evcon),
	    chan->name, sub->sender ? sub->sender : "*",
	    sub->type ? sub->type : "*");
	
	TAILQ_REMOVE(&chan->subs, sub, next);
	free(sub->sender);
	free(sub->type);
	free(sub);
	
	/* Close channel if we're the last subscriber */
	if (TAILQ_EMPTY(&chan->subs)) {
		SPLAY_REMOVE(msgbus_channel_tree, &msgbus_channels, chan);
		free(chan->name);
		free(chan);
	}
}

void
msgbus_sub_open(struct evhttp_request *req,
    const char *channel, const char *sender, const char *type)
{
	struct msgbus_channel *chan, find = { .name = (char *)channel };
	struct msgbus_sub *sub;

	/* Create channel if we're the first subscriber. */
	if ((chan = SPLAY_FIND(msgbus_channel_tree,
		 &msgbus_channels, &find)) == NULL) {
		chan = calloc(1, sizeof(*chan));
		chan->name = strdup(channel);
		TAILQ_INIT(&chan->subs);
		SPLAY_INSERT(msgbus_channel_tree, &msgbus_channels, chan);
	}
	sub = calloc(1, sizeof(*sub));
	sub->req = req;
	sub->channel = chan;
	if (sender != NULL) sub->sender = strdup(sender);
	if (type != NULL) sub->type = strdup(type);

	/* Clean up subscription on connection close. */
	evhttp_connection_set_closecb(req->evcon, msgbus_sub_close, sub);
	
	TAILQ_INSERT_TAIL(&chan->subs, sub, next);

	printf("SUB %s %s %s %s\n", _evhttp_peername(req->evcon),
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
	switch (req->type) {
	case EVHTTP_REQ_GET:
	{
		struct evbuffer *buf = evbuffer_new();
		struct evkeyvalq params;
	
		evhttp_parse_query(req->uri, &params);
		msgbus_sub_open(req, channel,
		    evhttp_find_header(&params, "sender"),
		    evhttp_find_header(&params, "type"));
		evhttp_clear_headers(&params);
		
		evhttp_add_header(req->output_headers, "Content-Type",
		    "multipart/x-mixed-replace;boundary=" BOUNDARY_MARKER);
		evbuffer_add_printf(buf, "--%s\n", BOUNDARY_MARKER);
		evhttp_send_reply_start(req, HTTP_OK, "OK");
		evhttp_send_reply_data(req, buf);
		evbuffer_free(buf);
		break;
	}
	case EVHTTP_REQ_POST:
	{
		const char *sender, *type;
		
		sender = auth_parse(evhttp_find_header(req->input_headers,
					"Authorization"));
		type = evhttp_find_header(req->input_headers, "Content-Type");

		printf("PUB %s %s %s %s (%ld)\n", _evhttp_peername(req->evcon),
		    channel, sender ? sender : "*", type ? type : "*",
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
			printf("DOC %s %s (%ld)\n",
			    _evhttp_peername(req->evcon), req->uri, len);
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
	
	if (req != NULL && req->evcon != NULL) {
		if (strncmp(req->uri, "/msgbus/", 8) == 0) {
			char *channel = strdup(req->uri + 8);
			strtok(channel, "?");
			msgbus_bus_handler(ctx, req, channel);
			free(channel);
		} else if (ctx->docroot != NULL) {
			msgbus_doc_handler(ctx, req);
		}
	} else {
		printf("got NULL request - connection_fail?\n");
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
