#!/usr/bin/env python

import os
import event, evmsg

def recv_chat(channel, type, sender, msg):
    print '> %s@%s <%s>: %r' % (sender, channel, type, str(msg))

def recv_stdin():
    buf = os.read(0, 1024)
    if not buf:
        event.abort()
    else:
        evmsg.publish('chat', 'text/html', buf.strip())
        return True

def main():
    # XXX - stdin b0rkage!
    os.putenv('EVENT_NOKQUEUE', '1')
    os.putenv('EVENT_NOPOLL', '1')
    
    event.init()
    event.read(0, recv_stdin)
    evmsg.open()
    sub = evmsg.subscribe('chat', '*', '*', recv_chat)
    event.signal(2, event.abort)
    event.dispatch()
    
if __name__ == '__main__':
    main()
