
#/* Copyright (C) 1991, 1990, 1993, 1994, 1995 Transarc Corporation - All rights reserved */

SHELL = /bin/sh
COMPONENT=vlserver
include ../config/Makefile.${SYS_NAME}
MKDIR_IF_NEEDED=[ -d $$1 ] || mkdir -p $$1

KERNELDIR=../libafs/
UKERNELDIR=../libuafs/
INSTALL=${SRCDIR}bin/install
COMPILE_ET = ${SRCDIR}bin/compile_et

CFLAGS = ${DBUG} -I. -I.. -I${SRCDIR}include ${XCFLAGS}
INCLS=${SRCDIR}include/ubik.h \
             ${SRCDIR}include/lwp.h \
             ${SRCDIR}include/lock.h  \
             ${SRCDIR}include/rx/rx.h \
             ${SRCDIR}include/rx/xdr.h \
             ${SRCDIR}include/afs/keys.h \
             ${SRCDIR}include/afs/cellconfig.h \
	vlserver.h vlclient.h vldbint.h

LIBS = \
	${SRCDIR}lib/libubik.a \
	${SRCDIR}lib/afs/libauth.a \
	${SRCDIR}lib/librxkad.a \
	${SRCDIR}lib/librxstat.a \
	${SRCDIR}lib/librx.a \
	${SRCDIR}lib/liblwp.a \
	${SRCDIR}lib/libdes.a \
	${SRCDIR}lib/afs/libcom_err.a \
	${SRCDIR}lib/afs/util.a \
	${SRCDIR}lib/afs/libsys.a \
	${SRCDIR}lib/afs/util.a

auditlib=${SRCDIR}lib/afs/libaudit.a

RXGEN=${SRCDIR}bin/rxgen
KSRCS=Kvldbint.cs.c Kvldbint.xdr.c vl_opcodes.h vldbint.h
UKSRCS=$(KSRCS)

OBJS=vldbint.xdr.o vldbint.cs.o vl_errors.o

CDIR=../../../src/${COMPONENT}
include ../config/Makefile.version

noversion: install

all: 

clean:
	$(RM) -f *.o vlserver vlclient cnvldb vldbint.cs.c vldbint.ss.c \
	vldbint.xdr.c vldbint.h Kvldbint.cs.c Kvldbint.xdr.c \
	core *.a *.otl vlserver.h vldb_check\
	vl_errors.c AFS_component_version_number.c

vldb_check: vldb_check.c
	$(CC) ${CFLAGS} -o vldb_check vldb_check.c ${XLIBS} \
		${DESTDIR}lib/afs/libcmd.a ${DESTDIR}lib/afs/util.a

cnvldb: cnvldb.o 
	$(CC) ${CFLAGS} -o cnvldb cnvldb.o ${XLIBS} ${DESTDIR}lib/afs/libcmd.a\
		 ${DESTDIR}lib/afs/util.a		 

cnvldb.o: cnvldb.c cnvldb.h AFS_component_version_number.o

sascnvldb: sascnvldb.o 
	$(CC) ${CFLAGS} -o sascnvldb sascnvldb.o ${XLIBS}

sascnvldb.o: sascnvldb.c cnvldb.h AFS_component_version_number.o

vlserver: vlserver.o vlutils.o vlprocs.o vldbint.ss.o vldbint.xdr.o $(LIBS)
	$(CC) ${CFLAGS} -o vlserver vlserver.o vlutils.o vlprocs.o vldbint.ss.o vldbint.xdr.o  $(LIBS) ${XLIBS} ${auditlib}

vlserver.o: vlserver.c ${INCLS} AFS_component_version_number.o
vlutils.o: vlutils.c ${INCLS}
vlprocs.o: vlprocs.c ${INCLS}
vldbint.ss.o: vldbint.ss.c vldbint.xdr.c
vldbint.cs.o: vldbint.cs.c vldbint.xdr.c
vldbint.xdr.o: vldbint.xdr.c vldbint.h
vldbint.ss.c vldbint.cs.c vldbint.xdr.c vldbint.h: vldbint.xg
	${RXGEN} -x vldbint.xg

Kvldbint.cs.c: vldbint.xg
	${RXGEN} -x -k -C -o $@ vldbint.xg
Kvldbint.xdr.c: vldbint.xg
	${RXGEN} -x -k -c -o $@ vldbint.xg

libvldb.a: $(OBJS) AFS_component_version_number.o
	-$(RM) -f libvldb.a
	$(AR) r libvldb.a $(OBJS) AFS_component_version_number.o
	$(RANLIB) libvldb.a

vlclient: vlclient.o libvldb.a $(LIBS)
	$(CC) ${CFLAGS} -o vlclient vlclient.o libvldb.a $(LIBS) ${XLIBS} ${DESTDIR}lib/afs/libcmd.a

vlclient.o: vlclient.c ${INCLS} AFS_component_version_number.o

vl_errors.o: vl_errors.c
vlserver.h vl_errors.c: vl_errors.et vlserver.p.h
	$(RM) -f vlserver.h vl_errors.c; ${COMPILE_ET} vl_errors -h vlserver

system: install

kinstall: $(KSRCS)
	${INSTALL} $(KSRCS) ${KERNELDIR}afsint

ukinstall webinstall: $(UKSRCS)
	set ${UKERNELDIR}afsint; $(MKDIR_IF_NEEDED)
	${INSTALL} $(UKSRCS) ${UKERNELDIR}afsint

${DESTDIR}root.server/usr/afs/bin/vlserver: vlserver
	${INSTALL} vlserver ${DESTDIR}root.server/usr/afs/bin

${DESTDIR}/etc/vldb_convert: cnvldb
	${INSTALL} -f cnvldb ${DESTDIR}/etc/vldb_convert

install: kinstall ukinstall vlserver vlclient libvldb.a cnvldb ${DESTDIR}root.server/usr/afs/bin/vlserver ${DESTDIR}/etc/vldb_convert vldb_check
	${INSTALL} libvldb.a ${DESTDIR}lib/afs
	${INSTALL} vl_opcodes.h vlserver.h vldbint.h ${DESTDIR}include/afs
	${INSTALL} cnvldb.h ${DESTDIR}include/afs
	${INSTALL} vldb_check ${DESTDIR}/etc

