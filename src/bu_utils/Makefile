# Copyright 2000, International Business Machines Corporation and others.
# All Rights Reserved.
# 
# This software has been released under the terms of the IBM Public
# License.  For details, see the LICENSE file in the top-level source
# directory or online at http://www.openafs.org/dl/license10.html

SHELL = /bin/sh
COMPONENT=bu_utils
include ../config/Makefile.${SYS_NAME}

INSTALL=${SRCDIR}bin/install
COMPILE_ET = ${DESTDIR}/bin/compile_et
CFLAGS= -g -w -I${SRCDIR}include -I${SRCDIR}include/afs ${XCFLAGS}
LDFLAGS = ${XLDFLAGS}

include ../config/Makefile.version

noversion: install

all: fms

FMSOBJS = fms.o
FMSLIBS=${DESTDIR}lib/afs/libcmd.a \
	${TXLIBS} \
        $(SRCDIR)lib/afs/libusd.a \
	$(SRCDIR)lib/afs/libtermlib.a ${XLIBS} \
	$(SRCDIR)lib/afs/util.a

fms:	${FMSOBJS}
	${CC} ${LDFLAGS} -o fms ${FMSOBJS} ${FMSLIBS}

fms.o:  fms.c AFS_component_version_number.o

#
# global operations 
#

system:	install

install:	all
	${INSTALL} fms ${DESTDIR}etc

clean:
	rm -f *.o fms AFS_component_version_number.c
