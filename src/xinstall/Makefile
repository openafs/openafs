# Make modified install
SHELL = /bin/sh
COMPONENT=xinstall
include ../config/Makefile.${SYS_NAME}

LIBS= ${XLIBS}
CFLAGS= ${OPTMZ} ${XCFLAGS}

include ../config/Makefile.version

xinstall: xinstall.o setmode.o
	$(CC) -g -I${DESTDIR}include ${CFLAGS} -o xinstall setmode.o xinstall.o ${LIBS}
	- [ -d test ] && ( cd test; $(MAKE) )
	- [ -d test ] && ./test/test_install

${DESTDIR}bin/install: xinstall
	./xinstall -f xinstall ${DESTDIR}bin/xinstall

# Note: Some rule needs to appear here so that OSF's make does not try to
# compile install from install.c. A ";" will not work since other platforms
# complain. That's why we use 'echo'.
install: ${DESTDIR}bin/xinstall
	@echo -n

noversion: install

kinstall ukinstall webinstall:
	@echo No kernel source here.

system: install

clean:
	rm -f pinstall



