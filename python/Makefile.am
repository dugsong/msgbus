
if HAVE_PYTHON
# XXX - nasty automake fudging here...

noinst_SCRIPTS = python-build

$(srcdir)/evmsg.c: $(srcdir)/evmsg.pyx
	cython $(srcdir)/evmsg.pyx

python-build: $(srcdir)/evmsg.c
	$(PYTHON) setup.py build
	touch python-build

python-clean:
	rm -rf *build

clean: python-clean

distclean: clean

python-install:
	$(PYTHON) setup.py install --prefix=$(DESTDIR)$(prefix)

install-exec-local: python-install

else

install-exec-local:

endif

EXTRA_DIST = evmsg.pyx evmsg.c setup.py.in msgchat.py msgdump.py msgdump2.py msgsend.py

DISTCLEANFILES = *~
