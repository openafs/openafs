#
# Makefile for the null library.
#
#	(C) Copyright Transarc Corporation 1989
#	Licensed Materials - Property of Transarc
#	All Rights Reserved.
#
DESTDIR=DEST/
SRCDIR=DEST/
INSTALL=${SRCDIR}bin/install
SHELL=/bin/sh

COMPONENT=null
include ../config/Makefile.${SYS_NAME}
include ../config/Makefile.version
CFLAGS = ${XCFLAGS}

all: libnull.a

null.o: null.c AFS_component_version_number.o

libnull.a: null.o
	-rm -f $@
	ar rv $@ null.o
	$(RANLIB) $@

system install noversion install.noversion version: all
	${INSTALL} libnull.a ${DESTDIR}lib/afs

clean:
	rm -f *.o *.a AFS_component_version_number.c
