# Copyright 2000, International Business Machines Corporation and others.
# All Rights Reserved.
# 
# This software has been released under the terms of the IBM Public
# License.  For details, see the LICENSE file in the top-level source
# directory or online at http://www.openafs.org/dl/license10.html

# Makefile for the null library.
DESTDIR=DEST/
SRCDIR=DEST/
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
