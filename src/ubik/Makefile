# Copyright 2000, International Business Machines Corporation and others.
# All Rights Reserved.
# 
# This software has been released under the terms of the IBM Public
# License.  For details, see the LICENSE file in the top-level source
# directory or online at http://www.openafs.org/dl/license10.html

SHELL = /bin/sh
COMPONENT=ubik
include ../config/Makefile.${SYS_NAME}
MKDIR_IF_NEEDED=[ -d $$1 ] || mkdir -p $$1

CFLAGS=${OPTMZ} -I${TOP_SRCDIR}/config -I./ -I${SRCDIR}include ${XCFLAGS}
UKERNELDIR=../libuafs/
LIBOBJS=disk.o  remote.o beacon.o recovery.o ubik.o  vote.o lock.o phys.o \
	ubik_int.cs.o ubik_int.ss.o ubik_int.xdr.o ubikcmd.o \
	ubikclient.o uerrors.o
INCLS= ${SRCDIR}include/lwp.h ${SRCDIR}include/lock.h \
	${SRCDIR}include/rx/rx.h ${SRCDIR}include/rx/xdr.h \
	${SRCDIR}include/lock.h ubik.h ubik_int.h
LIBS=${SRCDIR}lib/librx.a ${SRCDIR}lib/liblwp.a \
     ${SRCDIR}lib/afs/libcom_err.a ${SRCDIR}lib/afs/libcmd.a \
     ${SRCDIR}lib/afs/util.a ${SRCDIR}lib/afs/libsys.a ${XLIBS}
RXGEN=${SRCDIR}bin/rxgen
COMPILE_ET = ${SRCDIR}bin/compile_et

UKSRCS=ubik.h ubikclient.c uerrors.c

include ../config/Makefile.version

all: libubik.a test

test: utst_server utst_client udebug
	${INSTALL} udebug ${DESTDIR}bin
	${INSTALL} udebug ${DESTDIR}root.server/usr/afs/bin

clean:
	rm -f *.o utst_server utst_client udebug *.a *.cs.c *.ss.c *.xdr.c
	rm -f utst_int.h ubik_int.h uerrors.c ubik.h core AFS_component_version_number.c

system: install

kinstall:
	@echo No $@ source here.

ukinstall webinstall: ${UKSRCS} Kubik_int.h Kubik_int.xdr.c Kubik_int.cs.c
	set ${UKERNELDIR}afs; $(MKDIR_IF_NEEDED)
	set ${UKERNELDIR}afsint; $(MKDIR_IF_NEEDED)
	${INSTALL} Kubik_int.h ${UKERNELDIR}afsint/ubik_int.h
	${INSTALL} Kubik_int.cs.c ${UKERNELDIR}afsint/ubik_int.cs.c
	${INSTALL} Kubik_int.xdr.c ${UKERNELDIR}afsint/ubik_int.xdr.c
	${INSTALL} ${UKSRCS} ${UKERNELDIR}afs

beancount:
	wc *.[ch] *.xg

install: libubik.a ukinstall
	${INSTALL} libubik.a ${DESTDIR}lib
	${INSTALL} ubik.h ubik_int.h ${DESTDIR}include

utst_server: utst_server.o utst_int.ss.o utst_int.xdr.o libubik.a 
	${CC} ${CFLAGS} -o utst_server utst_server.o utst_int.ss.o utst_int.xdr.o libubik.a $(LIBS)

utst_client: utst_client.o utst_int.cs.o utst_int.xdr.o libubik.a 
	${CC} ${CFLAGS} -o utst_client utst_client.o utst_int.cs.o utst_int.xdr.o libubik.a $(LIBS)

udebug: udebug.o libubik.a 
	${CC} ${CFLAGS} -o udebug udebug.o libubik.a $(LIBS)

ubik_int.ss.c ubik_int.cs.c ubik_int.xdr.c ubik_int.h: ubik_int.xg
	${RXGEN} -x ubik_int.xg

Kubik_int.cs.c: ubik_int.xg Kubik_int.h
	${RXGEN} -x -k -C -o Kubik_int.cs.c ubik_int.xg
 
Kubik_int.xdr.c: ubik_int.xg
	${RXGEN} -x -k -c -o Kubik_int.xdr.c ubik_int.xg
 
Kubik_int.h: ubik_int.xg
	${RXGEN} -x -k -h -o Kubik_int.h ubik_int.xg

utst_int.ss.o: utst_int.ss.c utst_int.xdr.c
utst_int.cs.o: utst_int.cs.c utst_int.xdr.c
utst_int.xdr.o: utst_int.xdr.c utst_int.h
utst_int.rs.c utst_int.cs.c utst_int.xdr.c utst_int.h: utst_int.xg
	${RXGEN}  utst_int.xg

utst_server.o: utst_server.c ${INCLS} utst_int.h AFS_component_version_number.c
utst_client.o: utst_client.c ${INCLS} utst_int.h AFS_component_version_number.c

udebug.o: udebug.c ${INCLS} AFS_component_version_number.c
	${CC} ${CFLAGS} -g -I. -I${DESTDIR}/include -I${DESTDIR}/include/afs -I${DESTDIR}/ -c udebug.c

uerrors.o: uerrors.c

uerrors.c ubik.h: uerrors.et ubik.p.h
	rm -f ubik.h uerrors.c; ${COMPILE_ET} uerrors -h ubik

libubik.a: ${LIBOBJS} AFS_component_version_number.o
	-rm -f $@
	ar rv $@ ${LIBOBJS} AFS_component_version_number.o
	$(RANLIB) $@

disk.o: disk.c ${INCLS}
remote.o: remote.c ${INCLS}
beacon.o: beacon.c ${INCLS}
lock.o: lock.c ${INCLS}
recovery.o: recovery.c ${INCLS}
ubik.o: ubik.c ${INCLS}
phys.o: phys.c ${INCLS}
vote.o: vote.c ${INCLS}
ubikcmd.o: ubikcmd.c ${INCLS}
ubikclient.o: ubikclient.c ${INCLS}
ubik_int.ss.o: ubik_int.ss.c ${INCLS}
ubik_int.cs.o: ubik_int.cs.c ${INCLS}
ubik_int.xdr.o: ubik_int.xdr.c ${INCLS}
utst_int.ss.o: utst_int.ss.c ${INCLS} utst_int.h
utst_int.cs.o: utst_int.cs.c ${INCLS} utst_int.h
utst_int.xdr.o: utst_int.xdr.c ${INCLS} utst_int.h 





