#!/usr/bin/env python

# $Id$

import httplib, optparse, re, sys, time, urllib

def main():
    op = optparse.OptionParser(usage='%prog [OPTIONS] [URL]')
    op.add_option('-v', dest='verbose', help='enable verbose output',
                  action='store_true')
    op.add_option('-s', dest='sender',
                  help='subscribe to messages from this sender')
    op.add_option('-t', dest='type',
                  help='subscribe to messages of this type')
    opts, args = op.parse_args(sys.argv[1:])
    if not args:
        args.append('http://localhost:8888/msgbus/flood')

    l = [ args[0], '%s' % urllib.urlencode(dict(filter(lambda x: x[1],
            (('sender', opts.sender), ('type', opts.type))))) ]
    url = '?'.join(filter(None, l))

    f = urllib.urlopen(url)
    hdrs = f.info()
    mp = re.compile('^multipart/.*boundary="?([^;"\n]*)', re.I|re.S)
    boundary = '--' + mp.match(hdrs['content-type']).group(1)
    
    print >>sys.stderr, 'subscribed to', url
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
