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

def main():
    if len(sys.argv) > 1:
        url = sys.argv[1]
    else:
        url = 'http://localhost:8888/msgbus/chatdemo'

    p = urlparse.urlsplit(url)
    if not p.path.startswith('/msgbus/'):
        raise ValueError, 'invalid msgbus URL: %s' % url
    channel = p.path[8:]
    
    # XXX - stdin b0rkage!
    os.putenv('EVENT_NOKQUEUE', '1')
    os.putenv('EVENT_NOPOLL', '1')
    
    event.init()
    event.read(0, recv_stdin, channel)
    evmsg.open(p.hostname, p.port, p.scheme == 'https')
    if p.username and p.password:
        evmsg.set_auth(p.username, p.password)
    sub = evmsg.subscribe(channel, '*', '*', recv_chat)
    event.signal(2, event.abort)

    print 'pub/sub to', url
    event.dispatch()
    
if __name__ == '__main__':
    main()
