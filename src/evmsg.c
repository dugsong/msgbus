/* $Id$ */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/queue.h>

/* XXX - b64_ntop */
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>

#include <assert.h>
#include <event.h>
#include <evhttp.h>
#include <stdlib.h>
#include <string.h>

#include "evmsg.h"

struct evmsg_conn {
	struct evmsg_ctx	 *ctx;
	struct event		  ev;
	struct timeval		  tv;
	struct evhttp_connection *evcon;
	char			 *channel;
	struct evbuffer		 *uri;
	evmsg_subscribe_cb	  cb;
	void			 *arg;
	char			 *boundary;
	TAILQ_ENTRY(evmsg_conn)	  next;
};

struct evmsg_ctx {
	TAILQ_HEAD(, evmsg_conn)  conns;
	char			 *server;
	u_short			  port;
	int			  use_ssl;
	char 			 *auth;
} *static_ctx;

static void
__publish_cb(struct evhttp_request *req, void *arg)
{
	if (req == NULL) {
		fprintf(stderr, "NULL req - evhttp_connection_fail?\n");
		return;
	} else if (req->response_code >= 300) {
		fprintf(stderr, "%d: %s\n", req->response_code,
		    req->response_code_line);
		return;
	}
}

static void
__null_cb(struct evhttp_request *req, void *arg)
{
}

static void
__subscribe_cb(struct evhttp_request *req, void *arg)
{
	struct evmsg_conn *conn = arg;
	struct evbuffer *msg, *buf = req->input_buffer;
	struct evkeyvalq kv[1];
	char *p;
		
	if (conn->cb == NULL) {
		return;
	} else if (req == NULL) {
		fprintf(stderr, "NULL req - evhttp_connection_fail?\n");
		return;
	} else if (req->evcon == NULL) {
		fprintf(stderr, "NULL evcon - evhttp_connection_done?\n");
		return;
	} else if (req->response_code >= 300) {
		fprintf(stderr, "%d: %s\n", req->response_code,
		    req->response_code_line);
		return;
	}
	/* Parse the multipart boundary, once. */
	if (conn->boundary == NULL) {
		const char *t = evhttp_find_header(req->input_headers,
		    "Content-Type");
		/* XXX - multipart/x-mixed-replace;boundary=00MsgBus00 */
		t = strchr(t, '=') + 1;
		conn->boundary = malloc(2 + strlen(t) + 1);
		sprintf(conn->boundary, "--%s", t);
	}
	/* Parse buffer for internal hdrs */
	TAILQ_INIT(kv);
	while ((p = evbuffer_readline(buf)) != NULL && p[0] != '\0') {
		char *k = strsep(&p, ":");
		if (p != NULL &&
		    (strcasecmp(k, "Content-Type") == 0 ||
			strcasecmp(k, "From") == 0 ||
			strcasecmp(k, "Content-Location") == 0)) {
			p += strspn(p, " ");
			evhttp_add_header(kv, k, p);
		}
		free(k);
	}
	/*
	 * XXX - assume msgbus' chunks correspond to multipart
	 * boundaries (too chummy with the server implementation?)
	 */
	if ((p = (char *)evbuffer_find(buf, (u_char *)conn->boundary,
		 strlen(conn->boundary))) != NULL &&
	    evhttp_find_header(kv, "Content-Type") != NULL) {
		int n = (u_char *)p - EVBUFFER_DATA(buf);
		
		msg = evbuffer_new();
		evbuffer_add(msg, EVBUFFER_DATA(buf), n);
		evbuffer_expand(msg, n + 1);
		EVBUFFER_DATA(msg)[n] = '\0';
		
		p = conn->channel;
		if (*p == '\0') {
			p = (char *)evhttp_find_header(kv, "Content-Location");
			if (strncmp(p, "/msgbus/", 8) == 0)
				p += 8;
		}
		(*conn->cb)(p ? p : "", evhttp_find_header(kv, "Content-Type"),
		    evhttp_find_header(kv, "From"), msg, conn->arg);
		evbuffer_free(msg);
	}
	evhttp_clear_headers(kv);
}

static void
__subscribe_open(struct evhttp_connection *evcon, void *arg)
{
	struct evmsg_conn *conn = arg;
	struct evmsg_ctx *ctx = conn->ctx;
	struct evhttp_request *req;

#ifdef HAVE_OPENSSL
	if (ctx->use_ssl) {
		conn->evcon = evhttp_connection_new_ssl(ctx->server,
		    ctx->port);
	} else
#endif
	{
		conn->evcon = evhttp_connection_new(ctx->server, ctx->port);
	}
	evhttp_connection_set_timeout(conn->evcon, 0);
	evhttp_connection_set_retries(conn->evcon, -1);
	evhttp_connection_set_closecb(conn->evcon, __subscribe_open, conn);

	req = evhttp_request_new(__null_cb, conn);
	evhttp_request_set_chunked_cb(req, __subscribe_cb);
	evhttp_add_header(req->output_headers, "Host", ctx->server);
	evhttp_add_header(req->output_headers, "User-Agent", "libevmsg");
	if (ctx->auth != NULL) {
		evhttp_add_header(req->output_headers, "Authorization",
		    ctx->auth);
	}
	evhttp_make_request(conn->evcon, req, EVHTTP_REQ_GET,
	    (char *)EVBUFFER_DATA(conn->uri));
}

static void
__uri_escape(struct evbuffer *buf)
{
	char *p;
	
	evbuffer_add(buf, "", 1);
	p = evhttp_encode_uri((char *)EVBUFFER_DATA(buf));
	evbuffer_drain(buf, EVBUFFER_LENGTH(buf));
	evbuffer_add_printf(buf, "%s", p);
	evbuffer_add(buf, "", 1);
	free(p);
}

struct evmsg_ctx *
evmsg_ctx_open(const char *server, u_short port, int use_ssl)
{
	struct evmsg_ctx *ctx = calloc(1, sizeof(*ctx));
	struct evmsg_conn *conn;

	if (server == NULL)
		server = "127.0.0.1";
	if (port == 0)
		port = use_ssl ? EVMSG_DEFAULT_SSL_PORT : EVMSG_DEFAULT_PORT;
	ctx->server = strdup(server);
	ctx->port = port;
	ctx->use_ssl = use_ssl;
	
	/* First connection is for publishing */
	TAILQ_INIT(&ctx->conns);
	conn = calloc(1, sizeof(*conn));
	conn->ctx = ctx;
	conn->uri = evbuffer_new();
	if (ctx->use_ssl) {
#ifdef HAVE_OPENSSL
		conn->evcon = evhttp_connection_new_ssl(ctx->server,
		    ctx->port);
#else
		evmsg_ctx_close(&ctx);
		return (ctx);
#endif
	} else {
		conn->evcon = evhttp_connection_new(ctx->server, ctx->port);
	}
	evhttp_connection_set_retries(conn->evcon, -1);
	TAILQ_INSERT_HEAD(&ctx->conns, conn, next);

	return (ctx);
}

void
evmsg_open(const char *server, u_short port, int use_ssl)
{
	static_ctx = evmsg_ctx_open(server, port, use_ssl);
}

void
evmsg_ctx_set_auth(struct evmsg_ctx *ctx,
    const char *username, const char *password)
{
	struct evbuffer *tmp = evbuffer_new();
	int len;

	assert(username != NULL && password != NULL);
	free(ctx->auth);
	len = evbuffer_add_printf(tmp, "%s:%s", username, password);
	ctx->auth = malloc(6 + len * 2);
	strcpy(ctx->auth, "Basic ");
	b64_ntop(EVBUFFER_DATA(tmp), len, ctx->auth + 6, len * 2);
	evbuffer_free(tmp);
}

void
evmsg_set_auth(const char *username, const char *password)
{
	evmsg_ctx_set_auth(static_ctx, username, password);
}
	
int
evmsg_ctx_publish(struct evmsg_ctx *ctx,
    const char *channel, const char *type, struct evbuffer *msg)
{
	static char *buf;
	static int len;
	struct evmsg_conn *conn = TAILQ_FIRST(&ctx->conns);
	struct evhttp_request *req;
	int n = strlen(channel);
	
	if (len < n + 1) {
		len = 10 + n;
		free(buf);
		buf = malloc(len);
	}
	req = evhttp_request_new(__publish_cb, NULL);
	evhttp_add_header(req->output_headers, "Host", ctx->server);
	evhttp_add_header(req->output_headers, "User-Agent", "libevmsg");
	if (ctx->auth != NULL) {
		evhttp_add_header(req->output_headers, "Authorization",
		    ctx->auth);
	}
	evhttp_add_header(req->output_headers, "Content-Type", type);
	evbuffer_add_buffer(req->output_buffer, msg);
	evbuffer_drain(conn->uri, EVBUFFER_LENGTH(conn->uri));
	evbuffer_add_printf(conn->uri, "/msgbus/%s", channel);
	__uri_escape(conn->uri);
	
	return (evhttp_make_request(conn->evcon, req, EVHTTP_REQ_POST,
		    (char *)EVBUFFER_DATA(conn->uri)));
}

int
evmsg_publish(const char *channel, const char *type, struct evbuffer *msg)
{
	return (evmsg_ctx_publish(static_ctx, channel, type, msg));
}

void *
evmsg_ctx_subscribe(struct evmsg_ctx *ctx,
    const char *channel, const char *type, const char *sender,
    void (*callback)(const char *channel, const char *type, const char *sender,
	struct evbuffer *buf, void *arg), void *arg)
{
	char *qs;
	struct evmsg_conn *conn;
	
	conn = calloc(1, sizeof(*conn));
	conn->ctx = ctx;
	conn->uri = evbuffer_new();
	conn->cb = callback;
	conn->arg = arg;

	if (channel == NULL || strcmp(channel, "*") == 0)
		channel = "";
	conn->channel = strdup(channel);

	qs = evhttp_encode_uri(channel);
	evbuffer_add_printf(conn->uri, "/msgbus/%s?", qs);
	free(qs);

	qs = evhttp_encode_uri(type ? type : "*");
	evbuffer_add_printf(conn->uri, "type=%s&", qs);
	free(qs);

	qs = evhttp_encode_uri(sender ? sender : "*");
	evbuffer_add_printf(conn->uri, "sender=%s", qs);
	free(qs);

	TAILQ_INSERT_TAIL(&ctx->conns, conn, next);
	
	__subscribe_open(NULL, conn);

	return ((void *)conn);
}

void *
evmsg_subscribe(const char *channel, const char *type, const char *sender,
    void (*callback)(const char *channel, const char *type, const char *sender,
	struct evbuffer *buf, void *arg), void *arg)
{
	return (evmsg_ctx_subscribe(static_ctx,
		    channel, type, sender, callback, arg));
}

void
evmsg_ctx_unsubscribe(struct evmsg_ctx *ctx, void *arg)
{
	struct evmsg_conn *conn = arg;

	TAILQ_REMOVE(&ctx->conns, conn, next);
	evhttp_connection_set_closecb(conn->evcon, NULL, NULL);
	evhttp_connection_free(conn->evcon);
	if (conn->uri != NULL)
		evbuffer_free(conn->uri);
	free(conn->channel);
	free(conn);
}

void
evmsg_unsubscribe(void *arg)
{
	evmsg_ctx_unsubscribe(static_ctx, arg);
}

void
evmsg_ctx_close(struct evmsg_ctx **ctx)
{
	struct evmsg_conn *conn;

	/* Shutdown all our connections. */
	while ((conn = TAILQ_FIRST(&ctx[0]->conns)) != NULL) {
		evmsg_ctx_unsubscribe(ctx[0], conn);
	}
	free(ctx[0]->auth);
	free(ctx[0]->server);
	free(ctx[0]);
	ctx[0] = NULL;
}

void
evmsg_close(void)
{
	evmsg_ctx_close(&static_ctx);
}
