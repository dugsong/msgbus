/* $Id$ */

#ifndef EVMSG_H
#define EVMSG_H

#define EVMSG_DEFAULT_PORT	8888
#define EVMSG_DEFAULT_SSL_PORT	4444

/*
 * Setup to open a connection to a remote msgbus. Server and port may
 * be NULL and 0 to connect to the default local msgbus.
 */
void evmsg_open(const char *server, u_short port, int use_ssl);

/* Add credentials to connect with, if needed. */
void evmsg_set_auth(const char *username, const char *password);

/* Transfer ownership of a new message buffer to be sent (and freed). */
int evmsg_publish(const char *channel, const char *type, struct evbuffer *msg);

/*
 * Subscribe to a message stream. Sender and type may be comma-separated
 * wildcard patterns. Sender may be NULL, if the publisher didn't authenticate.
 * Returns an opaque handle which may be used to cancel the subscription.
 */
typedef void (*evmsg_subscribe_cb)(const char *channel, const char *type,
    const char *sender, struct evbuffer *buf, void *arg);

void *evmsg_subscribe(const char *channel, const char *type,const char *sender,
    evmsg_subscribe_cb callback, void *arg);

/* Cancel a subscription given its handle. */
void evmsg_unsubscribe(void *handle);

void evmsg_close(void);

/* Re-entrant version of the interface. */
typedef struct evmsg_ctx evmsg_ctx_t;

evmsg_ctx_t *evmsg_ctx_open(const char *server, u_short port, int use_ssl);

void evmsg_ctx_set_auth(evmsg_ctx_t *ctx,
    const char *username, const char *password);

int evmsg_ctx_publish(evmsg_ctx_t *ctx,
    const char *channel, const char *type, struct evbuffer *msg);

void *evmsg_ctx_subscribe(evmsg_ctx_t *ctx,
    const char *channel, const char *type,const char *sender,
    evmsg_subscribe_cb callback, void *arg);

void evmsg_ctx_unsubscribe(evmsg_ctx_t *ctx, void *handle);

void evmsg_ctx_close(evmsg_ctx_t **ctx);

#endif /* EVMSG_H */
