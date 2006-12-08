#!/usr/bin/env python

# $Id$

import base64, httplib, optparse, os, sys

def main():
    op = optparse.OptionParser(usage='%prog [OPTIONS] CHANNEL')
    op.add_option('-d', dest='address', default='127.0.0.1',
                  help='specify alternate destination address')
    op.add_option('-p', dest='port', default=8080, type='int',
                  help='specify alternate destination port')
    op.add_option('-f', dest='filename', help='specify file to data from')
    op.add_option('-a', dest='auth', help='username:password to send')
    op.add_option('-t', dest='type', help='content-type (default text/html)',
                  default='text/html')
    opts, args = op.parse_args(sys.argv[1:])
    if not args:
        op.error('no channel specified')
        
    # Read message data.
    f = opts.filename and open(opts.filename) or sys.stdin
    buf = f.read()

    # Send message.
    channel = ' '.join(args)
    uri = '/msgbus/' + ' '.join(args)
    hdrs = { 'Content-Type':opts.type, 'Content-Length':str(len(buf)),
             'Connection':'close' }
    if opts.auth:
        hdrs['Authorization'] = 'Basic %s' % \
            base64.encodestring(opts.auth).strip()

    conn = httplib.HTTPConnection('localhost:8080')
    conn.request('POST', uri, buf, hdrs)
    res = conn.getresponse()
    if res.status != 204:
        print res.status, res.reason
        print res.read()
    conn.close()

if __name__ == '__main__':
    main()
