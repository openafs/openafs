# Copyright 2000, International Business Machines Corporation and others.
# All Rights Reserved.
# 
# This software has been released under the terms of the IBM Public
# License.  For details, see the LICENSE file in the top-level source
# directory or online at http://www.openafs.org/dl/license10.html

SHELL = /bin/sh
DESTDIR=DEST/
SRCDIR=DEST/
BACKDIR=../../../../PARENT/dest/
OPTIMIZE=-O

LDIRS= -L${DESTDIR}lib -L${DESTDIR}lib/afs -L.. L${BACKDIR}lib -L${BACKDIR}lib/afs
LIBS=-lcmd -lcom_err ${DESTDIR}lib/afs/util.a

INCDIRS=  -I${SRCDIR}include -I${SRCDIR}include/afs -I.. -I${BACKDIR}include -I${BACKDIR}include/afs

CFLAGS = ${OPTIMIZE} ${INCDIRS} ${LDIRS} ${LIBS} ${XCFLAGS}

all: test
test tests: ctest dtest itest

itest: itest.o 
	cc ${CFLAGS} -o itest itest.o 

ctest: ctest.o
	cc ${CFLAGS} -o ctest ctest.o 

dtest: dtest.o
	cc ${CFLAGS} -o dtest dtest.o 

system: test

kinstall:
	@echo No kernel source here.

clean:
	rm -f  *.a *.o ctest dtest itest core

