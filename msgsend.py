#!/usr/bin/env python

# $Id$

import base64, httplib, optparse, sys, urlparse

def main():
    op = optparse.OptionParser(usage='%prog [OPTIONS] URL|channel')
    op.add_option('-f', dest='filename',
                  help='specify input file (default stdin)')
    op.add_option('-t', dest='type', help='content-type (default text/html)',
                  default='text/html')
    opts, args = op.parse_args(sys.argv[1:])
    if not args:
        op.error('missing URL or channel')

    if not args[0].startswith('http'):
        url = 'http://localhost:8888/msgbus/%s' % args[0]
    else:
        url = args[0]
        
    p = urlparse.urlsplit(url)
    if not p.path.startswith('/msgbus/'):
        raise ValueError, 'invalid msgbus URL: %s' % url
    
    f = opts.filename and open(opts.filename) or sys.stdin
    buf = f.read()
    hdrs = {
        'Content-Type':opts.type,
        'Content-Length':str(len(buf)),
        'Connection':'close'
        }
    if p.username and p.password:
        s = '%s:%s' % (p.username, p.password)
        hdrs['Authorization'] = 'Basic %s' % base64.encodestring(s.strip())

    if p.scheme == 'https':
        conn = httplib.HTTPSConnection('%s:%s' % (p.hostname, p.port or 4444))
    else:
        conn = httplib.HTTPConnection('%s:%s' % (p.hostname, p.port or 8888))
    conn.request('POST', p.path, buf, hdrs)
    res = conn.getresponse()
    if res.status != 204:
        raise RuntimeError, (res.status, res.reason)
    conn.close()

if __name__ == '__main__':
    main()
