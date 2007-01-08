/* $Id$ */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/tree.h>

#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>

#include <err.h>
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "event.h"
#include "evhttp.h"

#include "match.h"
#include "mimetype.h"

#define MAX_CLIENT_BUFSIZ	(1000 * 1000)

#define BOUNDARY_MARKER		"00MsgBus00"

struct msgbus_ctx {
	char		*address;
	int		 port;
	int		 ssl_port;
	const char	*certfile;
	const char	*docroot;
	const char	*secret;
	int		 verbose;
} ctx[1];

struct msgbus_channel;

struct msgbus_sub {
	struct evhttp_request	*req;
	char			*user;
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
SPLAY_HEAD(msgbus_channels, msgbus_channel) msgbus_channels = \
	SPLAY_INITIALIZER(&msgbus_channels);

struct msgbus_channel *msgbus_root;

static int
_channel_cmp(struct msgbus_channel *a, struct msgbus_channel *b)
{
	return (strcmp(a->name, b->name));
}
SPLAY_PROTOTYPE(msgbus_channels, msgbus_channel, next, _channel_cmp);
SPLAY_GENERATE(msgbus_channels, msgbus_channel, next, _channel_cmp);

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

static struct evbuffer *
_format_msg(const char *channel, const char *sender, const char *type,
    void *data, int len, int chunked)
{
	struct evbuffer *buf = evbuffer_new();
	
	if (sender != NULL)
		evbuffer_add_printf(buf, "From: %s\n", sender);
	if (channel != NULL)
		evbuffer_add_printf(buf, "Content-Location: /msgbus/%s\n",
		    channel);
	evbuffer_add_printf(buf, "Content-Type: %s\n", type);
	if (!chunked) {
		evbuffer_add_printf(buf,
		    "Content-Length: %d\n", len);
	}
	evbuffer_add(buf, "\n", 1);
	evbuffer_add(buf, data, len);
	evbuffer_add_printf(buf, "--%s\n", BOUNDARY_MARKER);
	return (buf);
}

static void
msgbus_deliver(struct msgbus_sub *sub,
    const char *channel, const char *type, const char *sender,
    void *buf, int len)
{
	struct evbuffer *msg;
	int n;

	n = evhttp_connection_write_pending(sub->req->evcon);
	if (n >= MAX_CLIENT_BUFSIZ) {
		fprintf(stderr, "dropping %s %s (%d) "
		    "from %s to %s:%d (%s)\n", channel, type, len, sender,
		    sub->req->remote_host, sub->req->remote_port, sub->user);
	} else {
		/* XXX - evbuffer_add_buffer() dirty swap requires new msg */
		msg = _format_msg(channel, sender, type,
		    buf, len, (sub->req->minor == 1));
		evhttp_send_reply_chunk(sub->req, msg);
		evbuffer_free(msg);
	}
}

static void
msgbus_dispatch(const char *channel, const char *type, const char *sender, 
    void *buf, int len)
{
	struct msgbus_channel *chan, find = { .name = (char *)channel };
	struct msgbus_sub *sub;
	
	if (msgbus_root != NULL) {
		TAILQ_FOREACH(sub, &msgbus_root->subs, next) {
			msgbus_deliver(sub, channel, type, sender, buf, len);
		}
	}
	if ((chan = SPLAY_FIND(msgbus_channels, &msgbus_channels, &find))) {
		TAILQ_FOREACH(sub, &chan->subs, next) {
			if ((sub->sender == NULL || sender == NULL ||
			     match_pattern_list(sender, sub->sender,
				 strlen(sub->sender), 0) == 1) &&
			    (sub->type == NULL ||
			     match_pattern_list(type, sub->type,
				 strlen(sub->type), 0) == 1)) {
				msgbus_deliver(sub, NULL, type, sender,
				    buf, len);
			}
		}
	}
}

static void
msgbus_sub_close(struct evhttp_connection *evcon, void *arg)
{
	struct msgbus_sub *sub = (struct msgbus_sub *)arg;
	struct msgbus_channel *chan = sub->channel;

	if (ctx->verbose > 0) {
		fprintf(stderr, "UNSUB %s (%s): %s %s %s\n",
		    _evhttp_peername(evcon), sub->user,
		    chan->name, sub->sender ? sub->sender : "*",
		    sub->type ? sub->type : "*");
	}
	TAILQ_REMOVE(&chan->subs, sub, next);
	free(sub->sender);
	free(sub->type);
	free(sub->user);
	free(sub);
	
	/* Close channel if we're the last subscriber */
	if (TAILQ_EMPTY(&chan->subs)) {
		SPLAY_REMOVE(msgbus_channels, &msgbus_channels, chan);
		if (msgbus_root == chan)
			msgbus_root = NULL;
		free(chan->name);
		free(chan);
	}
}

static void
msgbus_sub_open(struct evhttp_request *req, const char *user,
    const char *channel, const char *sender, const char *type)
{
	struct msgbus_channel *chan, find = { .name = (char *)channel };
	struct msgbus_sub *sub;

	/* Create channel if we're the first subscriber. */
	if ((chan = SPLAY_FIND(msgbus_channels,
		 &msgbus_channels, &find)) == NULL) {
		chan = calloc(1, sizeof(*chan));
		chan->name = strdup(channel);
		TAILQ_INIT(&chan->subs);
		SPLAY_INSERT(msgbus_channels, &msgbus_channels, chan);
		if (*channel == '\0')
			msgbus_root = chan;
	}
	sub = calloc(1, sizeof(*sub));
	sub->req = req;
	sub->user = user ? strdup(user) : strdup("?");
	sub->channel = chan;
	if (sender != NULL && strcmp(sender, "*") != 0)
		sub->sender = strdup(sender);
	if (type != NULL && strcmp(type, "*") != 0)
		sub->type = strdup(type);
	
	/* Clean up subscription on connection close. */
	evhttp_connection_set_closecb(req->evcon, msgbus_sub_close, sub);
	
	TAILQ_INSERT_TAIL(&chan->subs, sub, next);

	if (ctx->verbose > 0) {
		fprintf(stderr, "SUB %s (%s): %s %s %s\n",
		    _evhttp_peername(req->evcon), sub->user,
		    sub->channel->name, sub->sender ? sub->sender : "*",
		    sub->type ? sub->type : "*");
	}
}

static const char *
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

static void
msgbus_bus_handler(struct msgbus_ctx *ctx, struct evhttp_request *req,
    const char *channel, const char *sender)
{
	switch (req->type) {
	case EVHTTP_REQ_GET:
	{
		struct evbuffer *buf = evbuffer_new();
		struct evkeyvalq params;
	
		evhttp_parse_query(req->uri, &params);
		msgbus_sub_open(req, sender, channel,
		    evhttp_find_header(&params, "sender"),
		    evhttp_find_header(&params, "type"));
		evhttp_clear_headers(&params);
		
		evhttp_add_header(req->output_headers, "Content-Type",
		    "multipart/x-mixed-replace;boundary=" BOUNDARY_MARKER);
		evbuffer_add_printf(buf, "--%s\n", BOUNDARY_MARKER);
		evhttp_send_reply_start(req, HTTP_OK, "OK");
		evhttp_send_reply_chunk(req, buf);
		evbuffer_free(buf);
		break;
	}
	case EVHTTP_REQ_POST:
	{
		const char *type = evhttp_find_header(req->input_headers,
		    "Content-Type");

		if (type == NULL) {
			evhttp_send_error(req, HTTP_BADREQUEST, "Bad Request");
			return;
		}
		if (ctx->verbose > 2) {
			fprintf(stderr, "PUB %s (%s): %s %s (%ld)\n",
			    _evhttp_peername(req->evcon),
			    sender ? sender : "?", channel, type ? type : "*",
			    (long)EVBUFFER_LENGTH(req->input_buffer));
		}
		msgbus_dispatch(channel, type, sender,
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

static void
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
			if (ctx->verbose > 1) {
				fprintf(stderr, "DOC %s %s (%ld)\n",
				    _evhttp_peername(req->evcon),
				    req->uri, len);
			}
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

static void
msgbus_req_handler(struct evhttp_request *req, void *arg)
{
	struct msgbus_ctx *ctx = arg;
	const char *auth;
	char *user, *pass;
	
	if (req != NULL && req->evcon != NULL) {
		evhttp_add_header(req->output_headers, "Server", "msgbus");
		auth = evhttp_find_header(req->input_headers, "Authorization");
		user = pass = NULL;
		
		if ((auth = evhttp_find_header(req->input_headers,
			 "Authorization")) != NULL &&
		    strncasecmp(auth, "basic ", 6) == 0) {
			int n = ((strlen(auth) * 3) / 4) + 5;
			char *p = malloc(n);
			
			if ((n = b64_pton(auth + 6, (u_char *)p, n)) > 0) {
				p[n] = '\0';
				if ((pass = index(p, ':')) != NULL) {
					user = p;
					*pass++ = '\0';
				}
			} else
				free(p);
		}
		if (ctx->secret != NULL && (pass == NULL ||
			strcmp(ctx->secret, pass) != 0)) {
			struct evbuffer *buf = evbuffer_new();
			evhttp_add_header(req->output_headers,
			    "WWW-Authenticate", "Basic realm=\"msgbus\"");
			evbuffer_add_printf(buf, "<h1>Unauthorized</h1>");
			evhttp_send_reply(req, 401, "Unauthorized", buf);
		} else if (strncmp(req->uri, "/msgbus/", 8) == 0) {
			char *channel, *p = strdup(req->uri + 8);
			channel = strsep(&p, "?");
			msgbus_bus_handler(ctx, req, channel, user);
			free(channel);
		} else if (ctx->docroot != NULL) {
			msgbus_doc_handler(ctx, req);
		} else {
			struct evbuffer *buf = evbuffer_new();
			evbuffer_add_printf(buf, "<h1>Not Found</h1>");
			evhttp_send_reply(req, HTTP_NOTFOUND,
			    "Not Found", buf);
		}
		free(user);
	}
}

static void
usage(void)
{
	fprintf(stderr, "usage: msgbus [OPTIONS]\n\n"
	    "Options:\n"
	    "  -c CERTFILE   SSL PEM cert/key file (default: disabled)\n"
	    "  -d DIRECTORY  document root         (default: disabled)\n"
	    "  -l ADDRESS    address to listen on  (default: any)\n"
	    "  -p PORT       port to listen on     (default: 8888)\n"
	    "  -P SSL_PORT   SSL port to listen on (default: 4444)\n"
	    "  -s SECRET     server password       (default: none)\n"
	    "  -u USER       user/uid to run as    (default: none)\n"
	    "  -v[v[v]]      verbose mode          (default: none)\n"
	    );
	exit(1);
}

static void
ignore_cb(int sig, short what, void *arg)
{
}

int
main(int argc, char **argv)
{
	struct evhttp *httpd, *httpsd;
	struct rlimit fhqwhgads = { RLIM_INFINITY, RLIM_INFINITY };
	struct passwd *pwd = NULL;
	struct stat st;
	struct event pipe_ev;
	char path[MAXPATHLEN];
	int c;

	ctx->address = "0.0.0.0";
	ctx->port = 8888;
	ctx->ssl_port = 4444;
	
	while ((c = getopt(argc, argv, "c:d:l:p:P:s:u:vh?")) != -1) {
		switch (c) {
		case 'c':
			ctx->certfile = optarg;
			break;
		case 'd':
			ctx->docroot = realpath(optarg, path);
			break;
		case 'l':
			ctx->address = optarg;
			break;
		case 'p':
			ctx->port = atoi(optarg);
			break;
		case 'P':
			ctx->ssl_port = atoi(optarg);
			break;
		case 's':
			ctx->secret = optarg;
			break;
		case 'u':
			if (atoi(optarg) == 0) {
				pwd = getpwnam(optarg);
			} else {
				pwd = getpwuid(atoi(optarg));
			}
			if (pwd == NULL)
				errx(1, "unknown user/uid %s", optarg);
			break;
		case 'v':
			ctx->verbose++;
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

	/* Everybody to the limit! */
	setrlimit(RLIMIT_NOFILE, &fhqwhgads);
	
	if (ctx->docroot != NULL &&
	    (stat(ctx->docroot, &st) < 0 || !S_ISDIR(st.st_mode))) {
		errx(1, "invalid document root: %s", ctx->docroot);
	}
	event_init();

	/* Ignore SIGPIPE. */
	signal_set(&pipe_ev, SIGPIPE, ignore_cb, NULL);
	signal_add(&pipe_ev, NULL);
	
	/* Start HTTP server. */
	if ((httpd = evhttp_start(ctx->address, ctx->port)) != NULL) {
		evhttp_set_gencb(httpd, msgbus_req_handler, ctx);
		warnx("HTTP server on %s:%d", ctx->address, ctx->port);
	} else
		err(1, "evhttp_start");

	/* Start HTTPS server. */
	if (ctx->certfile != NULL) {
		if ((httpsd = evhttp_start_ssl(ctx->address, ctx->ssl_port,
			 ctx->certfile)) != NULL) {
			evhttp_set_gencb(httpsd, msgbus_req_handler, ctx);
			warnx("HTTPS server on %s:%d\n"
			    "server certificate = %s",
			    ctx->address, ctx->ssl_port, ctx->certfile);
		} else
			err(1, "evhttp_start_ssl");
	}
	if (pwd != NULL) {
		warnx("uid %d -> %d, gid %d -> %d",
		    getuid(), pwd->pw_uid, getgid(), pwd->pw_gid);
		if (setgid(pwd->pw_gid) < 0 || setuid(pwd->pw_uid) < 0)
			err(1, "setuid");
	}
	if (ctx->docroot != NULL)
		warnx("document root = %s", ctx->docroot);
	
	event_dispatch();
	
	/* NOTREACHED */
	evhttp_free(httpd);
	
	exit(1);
}
