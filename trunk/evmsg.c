
#include <sys/types.h>
#include <sys/time.h>
#include <sys/queue.h>

/* XXX - b64_ntop */
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>

#include <event.h>
#include <evhttp.h>
#include <stdlib.h>
#include <string.h>

#include "evmsg.h"

struct evmsg_conn {
	struct evhttp_connection *evcon;
	void (*cb)(const char *type, const char *sender,
	    struct evbuffer *msg, void *arg);
	void			 *arg;
	TAILQ_ENTRY(evmsg_conn)	  next;
};
TAILQ_HEAD(, evmsg_conn)  evmsg_conns;
char			 *evmsg_server;
u_short			  evmsg_port;
char 			 *evmsg_auth;

static void
__publish_cb(struct evhttp_request *req, void *arg)
{
}

static void
__subscribe_cb(struct evhttp_request *req, void *arg)
{
	static int boundary_len;
	struct evmsg_conn *conn = arg;
	struct evbuffer *buf = req->input_buffer;
	struct evkeyvalq kv[1];
	char *p;

	TAILQ_INIT(kv);

	/* Parse the multipart boundary, once. */
	if (boundary_len == 0) {
		const char *t = evhttp_find_header(req->input_headers,
		    "Content-Type");
		t = strchr(t, '=') + 1;
		boundary_len = 2 + strlen(t) + 1;
	}
	if (conn->cb != NULL) {
		/* Parse buffer for internal hdrs */
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
		 * boundaries (valid, but too cozy with the implementation)
		 */
		EVBUFFER_LENGTH(buf) -= boundary_len;
		EVBUFFER_DATA(buf)[EVBUFFER_LENGTH(buf)] = '\0';
		(*conn->cb)(evhttp_find_header(kv, "Content-Type"),
		    evhttp_find_header(kv, "From"), buf, conn->arg);
		evhttp_clear_headers(kv);
	}
}

void
evmsg_open(const char *server, u_short port)
{
	struct evmsg_conn *conn = calloc(1, sizeof(*conn));
	
	evmsg_server = strdup(server);
	evmsg_port = port;
	TAILQ_INIT(&evmsg_conns);
	conn->evcon = evhttp_connection_new(evmsg_server, evmsg_port);
	evhttp_connection_set_retries(conn->evcon, -1);
	TAILQ_INSERT_HEAD(&evmsg_conns, conn, next);
}

void
evmsg_set_auth(const char *username, const char *password)
{
	if (username != NULL && password != NULL) {
		struct evbuffer *tmp = evbuffer_new();
		int len = evbuffer_add_printf(tmp, "%s:%s",
		    username, password);
		evmsg_auth = malloc(len * 2);
		b64_ntop(EVBUFFER_DATA(tmp), len,
		    evmsg_auth, len * 2);
		evbuffer_free(tmp);
	}
}

int
evmsg_publish(const char *channel, const char *type, struct evbuffer *msg)
{
	static char *buf;
	static int len;
	struct evmsg_conn *conn = TAILQ_FIRST(&evmsg_conns);
	struct evhttp_request *req;
	int n = strlen(channel);
	
	if (len < n + 1) {
		len = 10 + n;
		free(buf);
		buf = malloc(len);
	}
	req = evhttp_request_new(__publish_cb, NULL);
	if (evmsg_auth != NULL) {
		evhttp_add_header(req->output_headers, "Authorization",
		    evmsg_auth);
	}
	evhttp_add_header(req->output_headers, "Content-Type", type);
	evbuffer_add_buffer(req->output_buffer, msg);
	snprintf(buf, len, "/msgbus/%s", channel);
	
	return (evhttp_make_request(conn->evcon, req, EVHTTP_REQ_POST, buf));
}

int
evmsg_subscribe(const char *channel, const char *type, const char *sender,
    void (*callback)(const char *type, const char *sender,
	struct evbuffer *buf, void *arg), void *arg)
{
	struct evmsg_conn *conn;
	struct evhttp_request *req;
	char buf[BUFSIZ];

	conn = calloc(1, sizeof(*conn));
	conn->evcon = evhttp_connection_new(evmsg_server, evmsg_port);
	conn->cb = callback;
	conn->arg = arg;
	evhttp_connection_set_retries(conn->evcon, -1);
	TAILQ_INSERT_TAIL(&evmsg_conns, conn, next);

	req = evhttp_request_new(__subscribe_cb, conn);
	
	snprintf(buf, sizeof(buf), "/msgbus/%s?type=%s&sender=%s",
	    channel, type ? type : "*", sender ? sender : "*");
	
	return (evhttp_make_request(conn->evcon, req, EVHTTP_REQ_GET, buf));
}

void
evmsg_close(void)
{
	struct evmsg_conn *conn;

	/* Shutdown all our connections. */
	while ((conn = TAILQ_FIRST(&evmsg_conns)) != NULL) {
		TAILQ_REMOVE(&evmsg_conns, conn, next);
		evhttp_connection_free(conn->evcon);
	}
	free(evmsg_auth);
	evmsg_auth = NULL;
	
	free(evmsg_server);
	evmsg_server = NULL;
}
