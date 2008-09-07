#!/bin/sh

if [ "x`uname -s`" = "xDarwin" ]; then
    aclocal && glibtoolize --automake && autoconf && autoheader && automake -fac
else
    autoreconf --force --install
fi
