
all: evmsg.c
	python setup.py build

evmsg.c: evmsg.pyx
	pyrexc evmsg.pyx

clean:
	rm -rf build

install:
	python setup.py install --home ~

