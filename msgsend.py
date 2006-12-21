#!/usr/bin/env python

# $Id$

import base64, httplib, optparse, sys, urlparse

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
        
    scheme, netloc, path, query, fragment = urlparse.urlsplit(url)
    if not path.startswith('/msgbus/'):
        raise ValueError, 'invalid msgbus URL: %s' % url
    channel = path[8:]
    username, password, hostname, port = parse_netloc(scheme, netloc)
    
    f = opts.filename and open(opts.filename) or sys.stdin
    buf = f.read()
    hdrs = {
        'Content-Type':opts.type,
        'Content-Length':str(len(buf)),
        'Connection':'close'
        }
    if username and password:
        s = '%s:%s' % (username, password)
        hdrs['Authorization'] = 'Basic %s' % base64.encodestring(s.strip())

    if scheme == 'https':
        conn = httplib.HTTPSConnection('%s:%s' % (hostname, port or 4444))
    else:
        conn = httplib.HTTPConnection('%s:%s' % (hostname, port or 8888))
    conn.request('POST', path, buf, hdrs)
    res = conn.getresponse()
    if res.status != 204:
        raise RuntimeError, (res.status, res.reason)
    conn.close()

if __name__ == '__main__':
    main()
