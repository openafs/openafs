#
#	(C) Copyright 10/17/86 by Carnegie Mellon University
#
SHELL = /bin/sh
DESTDIR=DEST/
SRCDIR=DEST/
CFLAGS= ${OPTMZ} ${XCFLAGS}
INSTALL=${SRCDIR}bin/install
LIBS = ${XLIBS}

PROGRAM=mpp
MANUAL=mpp.1

COMPONENT=mpp
include ../config/Makefile.${SYS_NAME}
include ../config/Makefile.version

system: install

all: ${PROGRAM}

install: ${PROGRAM} 
	${INSTALL} -c -s -m 755 ${PROGRAM} ${DESTDIR}etc

installdoc: ${MANUAL}
	${INSTALL} -c ${MANUAL} ${DESTDIR}man/man1

clean:
	-rm -f core a.out *.o *.out ${PROGRAM} AFS_component_version_number.c

${PROGRAM}: AFS_component_version_number.c
	${CC} ${CFLAGS} $@.c -o $@ ${LIBS}

mpp: mpp.c
