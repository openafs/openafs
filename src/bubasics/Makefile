# Copyright 2000, International Business Machines Corporation and others.
# All Rights Reserved.
# 
# This software has been released under the terms of the IBM Public
# License.  For details, see the LICENSE file in the top-level source
# directory or online at http://www.openafs.org/dl/license10.html

SHELL = /bin/sh
COMPONENT=bubasics
include ../config/Makefile.${SYS_NAME}

RXGEN=${SRCDIR}bin/rxgen
COMPILE_ET = ${SRCDIR}bin/compile_et

CFLAGS= ${DBUG} -I${TOP_SRCDIR}/config -I${SRCDIR}include ${XCFLAGS}

include ../config/Makefile.version

noversion: install

all: libbubasics.a bumon.h butc.h bubasics.h tcdata.h butm.h

OBJS=butc.ss.o butc.cs.o butc.xdr.o bumon.ss.o bumon.cs.o bumon.xdr.o butc_errs.o butm_errs.o butx_errs.o

libbubasics.a: $(OBJS) AFS_component_version_number.o
	-rm -f libbubasics.a
	ar r libbubasics.a ${OBJS} AFS_component_version_number.o
	${RANLIB} libbubasics.a

butc.xdr.c butc.ss.c butc.cs.c butc.h: butc.xg
	${RXGEN} butc.xg

bumon.xdr.c bumon.ss.c bumon.cs.c bumon.h: bumon.xg
	${RXGEN} bumon.xg

butm_errs.c butm.h: butm_errs.et butm.p.h
	rm -f butm.h butm_errs.c; ${COMPILE_ET} butm_errs -h butm

butc_errs.c tcdata.h: butc_errs.et tcdata.p.h butm.h
	rm -f tcdata.h butc_errs.c; ${COMPILE_ET} butc_errs -h tcdata

butx_errs.c butx.h: butx_errs.et
	rm -f butx.h butx_errs.c; ${COMPILE_ET} butx_errs -h butx

system: install

${DESTDIR}lib/afs/libbubasics.a: libbubasics.a 
	${INSTALL}   $? $@


install: all ${DESTDIR}lib/afs/libbubasics.a
	${INSTALL} bubasics.h butc.h bumon.h butm.h butx.h tcdata.h ${DESTDIR}include/afs

clean:
	-rm -f *.o *.a *.xdr.c *.ss.c *.cs.c \
butc.h butx.h butm.h bumon.h tcdata.h \
butc_errs.c butx_errs.c butm_errs.c \
core AFS_component_version_number.c
