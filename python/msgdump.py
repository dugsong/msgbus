#!/usr/bin/env python

# $Id$

import optparse, sys, time, urlparse
import event, evmsg

def recv_msg(channel, type, sender, msg, d):
    print '%s %s %s %s (%s)' % (time.time(), channel, type, sender, len(msg))
    if not d['quiet']:
        print `str(msg)`
    d['cnt'] += 1

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
    op = optparse.OptionParser(usage='%prog [OPTIONS] [URL|channel]')
    op.add_option('-q', dest='quiet', help='enable quiet output',
                  action='store_true')
    op.add_option('-s', dest='sender', default='*',
                  help='subscribe to messages from this sender')
    op.add_option('-t', dest='type', default='*',
                  help='subscribe to messages of this type')
    opts, args = op.parse_args(sys.argv[1:])
    if not args:
        url = 'http://localhost:8888/msgbus/'
        channel = ''
    elif args[0].startswith('http'):
        url = args[0]
        channel = url.split('/')[-1]
    else:
        url = 'http://localhost:8888/msgbus/%s' % args[0]
        channel = args[0]
        
    scheme, netloc, path, query, fragment = urlparse.urlsplit(url)
    if not path.startswith('/msgbus/'):
        raise ValueError, 'invalid msgbus URL: %s' % url
    channel = path[8:]
    username, password, hostname, port = parse_netloc(scheme, netloc)
    
    d = { 'quiet':opts.quiet, 'cnt':0 }
    start = time.time()
    
    event.init()

    evmsg.open(hostname, port, scheme == 'https')
    if username and password:
        print "set_auth", username, password
        evmsg.set_auth(username, password)
    sub = evmsg.subscribe(channel, opts.type, opts.sender, recv_msg, d)
    
    event.signal(2, event.abort)
    
    print >>sys.stderr, 'subscribed to', url
    event.dispatch()

    secs = time.time() - start
    print >>sys.stderr, 'received %d msgs over %d secs (%.1f mps)' % \
          (d['cnt'], secs, float(d['cnt']) / secs)

if __name__ == '__main__':
    main()
