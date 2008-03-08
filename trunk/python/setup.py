#!/usr/bin/env python

from distutils.core import setup, Extension

evmsg = Extension(name='evmsg',
                  sources=[ 'evmsg.c' ],
                  include_dirs=[ '..', '../src' ],
                  library_dirs=[ '../src/.libs' ],
                  libraries=[ 'event_core', 'event_extra', 'evmsg', 'ssl', 'crypto', 'resolv' ]
                  )

setup(name='evmsg',
      version='0.1',
      author='Dug Song',
      author_email='dugsong@monkey.org',
      description='msgbus event library',
      ext_modules = [ evmsg ])
