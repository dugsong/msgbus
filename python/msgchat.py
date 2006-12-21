#!/usr/bin/env python

import os, sys, urlparse
import event, evmsg

def recv_chat(channel, type, sender, msg):
    print '> %s@%s <%s>: %r' % (sender, channel, type, str(msg))

def recv_stdin(channel):
    buf = os.read(0, 1024)
    if not buf:
        event.abort()
    else:
        if buf.strip():
            evmsg.publish(channel, 'text/html', buf.strip())
        return True

# XXX - work around urlparse.urlsplit() < Python 2.5
def parse_netloc(scheme, netloc):
    l = netloc.split('@')
    if len(l) == 2:
        username, password = l.pop(0).split(':')
    else:
        username = password = None
    l = l[0].split(':')
    if len(l) == 2:
        hostname = l[0]
        port = int(l[1])
    else:
        hostname = l[0]
        port = scheme == 'https' and 443 or 80
    return username, password, hostname, port

def main():
    if len(sys.argv) > 1:
        url = sys.argv[1]
    else:
        url = 'http://localhost:8888/msgbus/chatdemo'

    scheme, netloc, path, query, fragment = urlparse.urlsplit(url)
    if not path.startswith('/msgbus/'):
        raise ValueError, 'invalid msgbus URL: %s' % url
    channel = path[8:]
    username, password, hostname, port = parse_netloc(scheme, netloc)

    # XXX - stdin b0rkage!
    os.putenv('EVENT_NOKQUEUE', '1')
    os.putenv('EVENT_NOPOLL', '1')
    
    event.init()
    event.read(0, recv_stdin, channel)
    evmsg.open(hostname, port, scheme == 'https')
    if username and password:
        evmsg.set_auth(username, password)
    sub = evmsg.subscribe(channel, '*', '*', recv_chat)
    event.signal(2, event.abort)

    print 'pub/sub to', url
    event.dispatch()
    
if __name__ == '__main__':
    main()
