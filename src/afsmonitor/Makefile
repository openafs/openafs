# Copyright 2000, International Business Machines Corporation and others.
# All Rights Reserved.
# 
# This software has been released under the terms of the IBM Public
# License.  For details, see the LICENSE file in the top-level source
# directory or online at http://www.openafs.org/dl/license10.html

# Makefile for Afsmonitor, the AFS Performance monitoring tool

SHELL = /bin/sh
DESTDIR=DEST/
SRCDIR=DEST/
COMPONENT=afsmonitor
include ../config/Makefile.${SYS_NAME}

INSTALL=${SRCDIR}bin/install

CFLAGS=-g -I. \
	-I${SRCDIR}include \
	-I${SRCDIR}include/afs \
	-I${SRCDIR} ${XCFLAGS}
LDFLAGS = ${XLDFLAGS}
RPCINCLS=

INCLS=${SRCDIR}include/afs/gtxobjects.h \
	${SRCDIR}include/afs/gtxwindows.h \
	${SRCDIR}include/afs/gtxcurseswin.h \
	${SRCDIR}include/afs/gtxdumbwin.h \
	${SRCDIR}include/afs/gtxX11win.h \
	${SRCDIR}include/afs/gtxobjects.h \
	${SRCDIR}include/afs/gtxlightobj.h \
	${SRCDIR}include/afs/gtxtextobj.h \
	${SRCDIR}include/afs/gtxobjdict.h \
	${RPCINCLS} \
	${SRCDIR}include/afs/keys.h \
	${SRCDIR}include/afs/cellconfig.h \
	${SRCDIR}include/afs/cmd.h \
	${SRCDIR}include/afs/xstat_fs.h \
	${SRCDIR}include/afs/xstat_cm.h \
	afsmonitor.h \
	afsmon-labels.h
LIBS=${SRCDIR}lib/afs/libxstat_fs.a \
	${SRCDIR}lib/afs/libxstat_cm.a \
	${SRCDIR}lib/afs/libgtx.a \
	${SRCDIR}lib/afs/libafsint.a \
	${SRCDIR}lib/afs/libcmd.a \
	${SRCDIR}lib/librx.a \
	${SRCDIR}lib/liblwp.a \
	${SRCDIR}lib/afs/libsys.a \
	${SRCDIR}lib/afs/util.a 

EXTRA_LIBS= ${TXLIBS} ${SRCDIR}lib/afs/libtermlib.a  ${XLIBS} 

include ../config/Makefile.version

noversion: install

all: afsmonitor 

afsmon-output.o: afsmon-output.c ${INCLS}
afsmon-win.o: afsmon-win.c ${INCLS}
afsmonitor.o: afsmonitor.c ${INCLS} AFS_component_version_number.c

afsmonitor: afsmonitor.o afsmon-win.o afsmon-output.o ${LIBS}
	${CC} ${LDFLAGS} -o afsmonitor afsmonitor.o afsmon-win.o \
		afsmon-output.o ${LIBS} ${EXTRA_LIBS}

afsmon-parselog:  afsmon-parselog.o ${LIBS}
	${CC} ${CFLAGS} -o afsmon-parselog afsmon-parselog.c \
		${LIBS} ${EXTRA_LIBS}

#gtxtest.o:	gtxtest.c ${INCLS}
#gtxtest:	gtxtest.o ${LIBS}
#		cc ${CFLAGS} -o gtxtest gtxtest.o ${LIBS} ${EXTRA_LIBS} 

#
# Misc others
#
system: install

install: all
	${INSTALL} afsmonitor ${DESTDIR}bin

clean:
	rm -f  *.o afsmonitor AFS_component_version_number.c

