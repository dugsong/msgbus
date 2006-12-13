
#ifndef EVMSG_H
#define EVMSG_H

/* Open a connection to a remote msgbus. */
void evmsg_open(const char *server, u_short port);

/* Add credentials to send. */
void evmsg_set_auth(const char *username, const char *password);

/* Transfer ownership of a new message buffer to be sent (and free'd). */
int evmsg_publish(const char *channel, const char *type, struct evbuffer *msg);

/*
 * Subscribe to a message stream. Sender and type may be comma-separated
 * wildcard patterns. Sender may be NULL, if the publisher didn't authenticate.
 XXX - need a way to unsub. also to restart sub on connection death.
 */
void evmsg_subscribe(const char *channel, const char *type, const char *sender,
    void (*callback)(const char *type, const char *sender,
	struct evbuffer *buf, void *arg), void *arg);

void evmsg_close(void);

#endif /* EVMSG_H */
