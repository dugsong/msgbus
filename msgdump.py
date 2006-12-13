#!/usr/bin/env python

# $Id$

import httplib, optparse, re, sys, time, urllib

def main():
    op = optparse.OptionParser(usage='%prog [OPTIONS] CHANNEL')
    op.add_option('-v', dest='verbose', help='enable verbose output',
                  action='store_true')
    op.add_option('-d', dest='address', default='127.0.0.1',
                  help='specify alternate destination address')
    op.add_option('-p', dest='port', default=8888, type='int',
                  help='specify alternate destination port')
    op.add_option('-s', dest='sender',
                  help='subscribe to messages from this sender')
    op.add_option('-t', dest='type',
                  help='subscribe to messages of this type')
    opts, args = op.parse_args(sys.argv[1:])
    if not args:
        op.error('no channel specified')
        
    channel = ' '.join(args)
    uri = '/msgbus/%s?%s' % (channel, urllib.urlencode(dict(filter(
        lambda x: x[1], (('sender', opts.sender), ('type', opts.type))))))
    try:
        h = httplib.HTTP(host=opts.address, port=opts.port)
        h.putrequest('GET', uri)
        h.endheaders()
        status, reason, hdrs = h.getreply()
    except httplib.socket.error, msg:
        raise SystemExit, 'connect to %s:%s: %s' % \
              (opts.address, opts.port, msg[1])
    
    if status != 200:
        raise SystemExit, 'HTTP %s %s' % (status, reason)
    mp = re.compile('^multipart/.*boundary="?([^;"\n]*)', re.I|re.S)
    boundary = '--' + mp.match(hdrs['content-type']).group(1)
    
    f = h.getfile()
    try:
        while 1:
            assert f.readline().strip() == boundary
            msg = httplib.HTTPMessage(f, 0)
            buf = f.read(int(msg['content-length']))
            print '%s %s %s (%s)' % (time.time(), msg.get('from', '?'),
                                     msg['content-type'], len(buf))
            if opts.verbose:
                print `buf`
    except KeyboardInterrupt:
        print >>sys.stderr, 'exiting at user request'

if __name__ == '__main__':
    main()
