#
# pyevmsg.pyx
#
# $Id$

cdef extern from "Python.h":
    object	PyBuffer_FromMemory(char *s, int len)

cdef extern from "event.h":
    struct evbuffer:
        int	__xxx
    evbuffer	*evbuffer_new()
    int		 evbuffer_add(evbuffer *buf, char *s, int len)
    char	*EVBUFFER_DATA(evbuffer *buf)
    int		 EVBUFFER_LENGTH(evbuffer *buf)
    
EVMSG_DEFAULT_PORT	= 8888
EVMSG_DEFAULT_SSL_PORT	= 4444

ctypedef void (*evmsg_subscribe_cb)(char *channel, char *type, char *sender,
                                    evbuffer *buf, void *arg)

cdef extern from "evmsg.h":
    void	 evmsg_open(char *server, unsigned short port, int use_ssl)
    void	 evmsg_set_auth(char *username, char *password)
    int		 evmsg_publish(char *channel, char *type, evbuffer *msg)
    void	*evmsg_subscribe(char *channel, char *type, char *sender,
                                 evmsg_subscribe_cb callback, void *arg)
    void	 evmsg_unsubscribe(void *handle)
    void	 evmsg_close()


cdef void __subscribe_cb(char *channel, char *type, char *sender,
                         evbuffer *buf, void *arg):
    sub = <object>arg
    if sender == NULL:
        s = None
    else:
        s = sender
    sub.callback(channel, type, s,
                 PyBuffer_FromMemory(EVBUFFER_DATA(buf), EVBUFFER_LENGTH(buf)),
                 *sub.args)

def open(server=None, port=None, use_ssl=False):
    """Open our msgbus connection."""
    if server is None:
        server = '127.0.0.1'
    if port is None:
        if use_ssl:
            port = EVMSG_DEFAULT_PORT_SSL
        else:
            port = EVMSG_DEFAULT_PORT
    evmsg_open(server, port, use_ssl)

def set_auth(username, password):
    """Set a username and password to connect with."""
    evmsg_set_auth(username, password)

def publish(channel, type, msg):
    """Publish a message."""
    cdef evbuffer *buf
    buf = evbuffer_new()
    evbuffer_add(buf, msg, len(msg))
    evmsg_publish(channel, type, buf)

cdef class subscribe:
    """subscribe(channel, type, sender, callback, *args) -> sub object

    Subscribe to a message stream. Returns a subscription object which
    may be deleted to unsubscribe.

    Arguments:

    channel  -- channel to subscribe to
    type     -- types to subscribe to, as comma-separated wildcard patterns
    sender   -- senders to subscribe to, as comma-separated wildcard patterns
    callback -- message callback with (channel, type, sender, msg, *args) prototype
    *args    -- optional callback arguments
    """
    cdef void		*handle
    cdef public object	 callback, args
    
    def __init__(self, channel, type, sender, callback, *args):
        self.callback = callback
        self.args = args
        self.handle = evmsg_subscribe(channel, type, sender,
                                      __subscribe_cb, <void *>self)
        print "SUB"

    def __dealloc__(self):
        print "UNSUB"
        evmsg_unsubscribe(self.handle)

def close():
    """Close our msgbus connection."""
    evmsg_close()
