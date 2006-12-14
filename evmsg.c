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
	struct event		  ev;
	struct timeval		  tv;
	struct evhttp_connection *evcon;
	char			 *channel;
	struct evbuffer		 *uri;
	evmsg_subscribe_cb	  cb;
	void			 *arg;
	int			  boundary_len;
	TAILQ_ENTRY(evmsg_conn)	  next;
};
struct evmsg_ctx {
	TAILQ_HEAD(, evmsg_conn)  conns;
	char			 *server;
	u_short			  port;
	int			  use_ssl;
	char 			 *auth;
} ctx[1];

static void
__publish_cb(struct evhttp_request *req, void *arg)
{
}

static void
__subscribe_cb(struct evhttp_request *req, void *arg)
{
	struct evmsg_conn *conn = arg;
	
	if (req == NULL || req->evcon == NULL) {
		return;
	}
	/* Parse the multipart boundary, once. */
	if (conn->boundary_len == 0) {
		const char *t = evhttp_find_header(req->input_headers,
		    "Content-Type");
		t = strchr(t, '=') + 1;
		conn->boundary_len = 2 + strlen(t) + 1;
	}
	if (conn->cb != NULL) {
		/* Parse buffer for internal hdrs */
		struct evbuffer *buf = req->input_buffer;
		struct evkeyvalq kv[1];
		char *p;
		
		TAILQ_INIT(kv);
		while ((p = evbuffer_readline(buf)) != NULL && p[0] != '\0') {
			char *k = strsep(&p, ":");
			if (p != NULL && (strcasecmp(k, "Content-Type") == 0 ||
				strcasecmp(k, "From") == 0)) {
				p += strspn(p, " ");
				evhttp_add_header(kv, k, p);
			}
			free(k);
		}
		/*
		 * XXX - assume msgbus' chunks correspond to multipart
		 * boundaries (valid, but chummy with the implementation)
		 */
		EVBUFFER_LENGTH(buf) -= conn->boundary_len;
		EVBUFFER_DATA(buf)[EVBUFFER_LENGTH(buf)] = '\0';
		if (evhttp_find_header(kv, "Content-Type") != NULL) {
			const char *chan = conn->channel;
			if (*chan == '\0') {
				chan = evhttp_find_header(kv,
				    "Content-Location");
			}
			(*conn->cb)(chan,
			    evhttp_find_header(kv, "Content-Type"),
			    evhttp_find_header(kv, "From"), buf, conn->arg);
		}
		evhttp_clear_headers(kv);
	}
}

void
__subscribe_open(struct evhttp_connection *evcon, void *arg)
{
	struct evmsg_conn *conn = arg;
	struct evhttp_request *req;

	if (ctx->use_ssl)
		conn->evcon = evhttp_connection_new_ssl(ctx->server, ctx->port);
	else
		conn->evcon = evhttp_connection_new(ctx->server, ctx->port);
	evhttp_connection_set_timeout(conn->evcon, 0);
	evhttp_connection_set_retries(conn->evcon, -1);
	evhttp_connection_set_closecb(conn->evcon, __subscribe_open, conn);

	req = evhttp_request_new(__subscribe_cb, conn);
	
	evhttp_make_request(conn->evcon, req, EVHTTP_REQ_GET,
	    (char *)EVBUFFER_DATA(conn->uri));
}

void
__uri_escape(struct evbuffer *buf)
{
	char *p = evhttp_encode_uri((char *)EVBUFFER_DATA(buf));
	evbuffer_drain(buf, EVBUFFER_LENGTH(buf));
	evbuffer_add_printf(buf, "%s", p);
	free(p);
}

void
evmsg_open(const char *server, u_short port, int use_ssl)
{
	struct evmsg_conn *conn;

	if (server == NULL)
		server = "127.0.0.1";
	if (port == 0)
		port = EVMSG_DEFAULT_PORT;
	ctx->server = strdup(server);
	ctx->port = port;
	ctx->use_ssl = use_ssl;
	
	/* First connection is for publishing */
	TAILQ_INIT(&ctx->conns);
	conn = calloc(1, sizeof(*conn));
	conn->uri = evbuffer_new();
	if (ctx->use_ssl)
		conn->evcon = evhttp_connection_new_ssl(ctx->server, ctx->port);
	else
		conn->evcon = evhttp_connection_new(ctx->server, ctx->port);
	evhttp_connection_set_retries(conn->evcon, -1);
	TAILQ_INSERT_HEAD(&ctx->conns, conn, next);
}

void
evmsg_set_auth(const char *username, const char *password)
{
	struct evbuffer *tmp;
	int len;

	assert(username != NULL && password != NULL);
	tmp = evbuffer_new();
	len = evbuffer_add_printf(tmp, "%s:%s", username, password);
	ctx->auth = malloc(len * 2);
	b64_ntop(EVBUFFER_DATA(tmp), len,
	    ctx->auth, len * 2);
	evbuffer_free(tmp);
}

int
evmsg_publish(const char *channel, const char *type, struct evbuffer *msg)
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

void *
evmsg_subscribe(const char *channel, const char *type, const char *sender,
    void (*callback)(const char *channel, const char *type, const char *sender,
	struct evbuffer *buf, void *arg), void *arg)
{
	struct evmsg_conn *conn;
	
	conn = calloc(1, sizeof(*conn));
	conn->uri = evbuffer_new();
	conn->channel = strdup(channel);
	evbuffer_add_printf(conn->uri, "/msgbus/%s?type=%s&sender=%s",
	    channel, type ? type : "*", sender ? sender : "*");
	__uri_escape(conn->uri);
	conn->cb = callback;
	conn->arg = arg;
	TAILQ_INSERT_TAIL(&ctx->conns, conn, next);
	
	__subscribe_open(NULL, conn);

	return ((void *)conn);
}

void
evmsg_unsubscribe(void *arg)
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
evmsg_close(void)
{
	struct evmsg_conn *conn;

	/* Shutdown all our connections. */
	while ((conn = TAILQ_FIRST(&ctx->conns)) != NULL) {
		evmsg_unsubscribe(conn);
	}
	free(ctx->auth);
	free(ctx->server);
	memset(ctx, 0, sizeof(*ctx));
}
