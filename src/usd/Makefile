# Copyright 2000, International Business Machines Corporation and others.
# All Rights Reserved.
# 
# This software has been released under the terms of the IBM Public
# License.  For details, see the LICENSE file in the top-level source
# directory or online at http://www.openafs.org/dl/license10.html

SHELL = /bin/sh
COMPONENT=ptserver
include ../config/Makefile.${SYS_NAME}

INSTALL=${SRCDIR}bin/install
RXGEN=${SRCDIR}bin/rxgen
COMPILE_ET = ${SRCDIR}bin/compile_et

CFLAGS = $(DBG) -I${SRCDIR}include ${XCFLAGS}

#####################################################################
# include files
INCLS=${SRCDIR}include/afs/param.h \
	usd.h

LINCLS=usd.h

#####################################################################
include ../config/Makefile.version

clean:
	$(RM) -f *.o libusd.a   \
	 core  AFS_component_version_number.c

########################################################################
# build libusd.a
LIBOBJS=usd_file.o

libusd.a: ${LIBOBJS} AFS_component_version_number.o
	$(RM) -f libusd.a
	$(AR) r libusd.a ${LIBOBJS} AFS_component_version_number.o
	$(RANLIB) libusd.a

usd_file.o: usd_file.c $(LINCLS)

#######################################################################
system: install

kinstall:
	@echo No kernel source here.


${DESTDIR}lib/afs/libusd.a: libusd.a
	${INSTALL}   $? $@


install: ${DESTDIR}lib/afs/libusd.a 
	${INSTALL} usd.h ${DESTDIR}include/afs
