noinst_SCRIPTS = python-build

$(srcdir)/evmsg.c: $(srcdir)/evmsg.pyx
	cython $(srcdir)/evmsg.pyx

python-build: $(srcdir)/evmsg.c
	python setup.py build
	touch python-build

python-clean:
	rm -rf *build

clean: python-clean

distclean: clean

python-install:
	python setup.py install --prefix=$(DESTDIR)$(prefix)

install-exec-local: python-install

DISTCLEANFILES = *~
