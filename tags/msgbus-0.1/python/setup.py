#!/usr/bin/env python

from distutils.core import setup, Extension

evmsg = Extension(name='evmsg',
                  sources=[ 'evmsg.c' ],
                  include_dirs=[ '..', '../libevent' ],
                  extra_objects=[ '../evmsg.o' ],
                  library_dirs=[ '../libevent/.libs' ],
                  libraries=[ 'evhttp', 'resolv' ]
                  )

setup(name='evmsg',
      version='0.1',
      author='Dug Song',
      author_email='dugsong@monkey.org',
      description='msgbus event library',
      ext_modules = [ evmsg ])
