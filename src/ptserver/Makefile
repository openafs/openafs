
#/* Copyright (C) 1990, 1989 Transarc Corporation - All rights reserved */
#
# (C) COPYRIGHT IBM CORPORATION 1988
# LICENSED MATERIALS - PROPERTY OF IBM
#

SHELL = /bin/sh
COMPONENT=ptserver
include ../config/Makefile.${SYS_NAME}

UKERNELDIR=../libuafs/
INSTALL=${SRCDIR}bin/install
RXGEN=${SRCDIR}bin/rxgen
COMPILE_ET = ${SRCDIR}bin/compile_et
MKDIR_IF_NEEDED=[ -d $$1 ] || mkdir -p $$1

CFLAGS = $(DBUG) -I${SRCDIR}include ${XCFLAGS}
PE = ../permit_xprt.h
INCLS=${SRCDIR}include/ubik.h \
	${SRCDIR}include/lock.h  \
	${SRCDIR}include/lwp.h \
	${SRCDIR}include/rx/rx.h \
	${SRCDIR}include/rx/xdr.h \
	${SRCDIR}include/afs/keys.h \
	${SRCDIR}include/afs/cellconfig.h \
	ptserver.h ptclient.h ptint.h ptopcodes.h pterror.h

LINCLS=${SRCDIR}include/ubik.h \
	${SRCDIR}include/lock.h \
	${SRCDIR}include/rx/rx.h \
	${SRCDIR}include/rx/xdr.h  \
	${SRCDIR}include/rx/rxkad.h \
	${SRCDIR}include/afs/auth.h \
	${SRCDIR}include/afs/cellconfig.h \
	ptclient.h ptint.h pterror.h

LIBS = 	${SRCDIR}lib/libubik.a \
	${SRCDIR}lib/afs/libauth.a \
	${SRCDIR}lib/librxkad.a \
	${SRCDIR}lib/librxstat.a \
	${SRCDIR}lib/librx.a \
	${SRCDIR}lib/liblwp.a \
	${SRCDIR}lib/libdes.a \
	${SRCDIR}lib/afs/libcom_err.a \
	${SRCDIR}lib/afs/util.a \
	${SRCDIR}lib/afs/libsys.a 

auditlib=${SRCDIR}lib/afs/libaudit.a

LIBOBJS=ptuser.o pterror.o ptint.cs.o ptint.xdr.o
 
UKSRCS=ptuser.c ptclient.h pterror.c pterror.h ptserver.h

include ../config/Makefile.version


clean:
	$(RM) -f *.o ptserver ptint.cs.c ptint.ss.c ptclient ptint.xdr.c ptint.h \
	libprot.a  pts readgroup readpwd db_verify testpt pterror.h pterror.c \
	 core  AFS_component_version_number.c

ptserver: ptserver.o ptutils.o ptprocs.o ptint.ss.o ptint.xdr.o utils.o $(LIBS) ${auditlib}
	$(CC) ${CFLAGS} -o ptserver ptserver.o ptutils.o ptprocs.o  ptint.ss.o ptint.xdr.o utils.o $(LIBS) ${XLIBS} ${auditlib}

ptserver.o: ptserver.c ${INCLS} AFS_component_version_number.c

ptutils.o: ptutils.c ${INCLS}

ptprocs.o: ptprocs.c ${INCLS}

utils.o: utils.c ${INCLS} 

ptint.ss.o: ptint.ss.c ptint.xdr.c ptint.xg
ptint.cs.o: ptint.cs.c ptint.xdr.c ptint.xg
ptint.xdr.o: ptint.xdr.c ptint.h ptint.xg

ptint.ss.c ptint.cs.c ptint.xdr.c ptint.h: ptint.xg
	${RXGEN} -x ptint.xg

Kptint.cs.c: ptint.xg Kptint.h
	${RXGEN} -x -k -C -o Kptint.cs.c ptint.xg
 
Kptint.xdr.c: ptint.xg
	${RXGEN} -x -k -c -o Kptint.xdr.c ptint.xg
 
Kptint.h: ptint.xg
	${RXGEN}  -x -k -h -o Kptint.h ptint.xg

display.o: display.c ${INCLS}

db_verify.o: db_verify.c ${INCLS} AFS_component_version_number.c

db_verify: db_verify.o pterror.o display.o $(LIBS)
	$(CC) ${CFLAGS} -o db_verify db_verify.o display.o pterror.o $(LIBS) ${XLIBS} ${SRCDIR}lib/afs/libcmd.a

ptclient: ptclient.o display.o libprot.a $(LIBS)
	$(CC) ${CFLAGS} -o ptclient ptclient.o display.o libprot.a $(LIBS) ${XLIBS}

ptclient.o: ptclient.c ${INCLS} AFS_component_version_number.c

ptuser.o: ptuser.c ${LINCLS} ${PE}

pterror.h pterror.c: pterror.et
	$(RM) -f pterror.h pterror.c; ${COMPILE_ET} pterror

libprot.a: ${LIBOBJS} AFS_component_version_number.o
	$(RM) -f libprot.a
	$(AR) r libprot.a ${LIBOBJS} AFS_component_version_number.o
	$(RANLIB) libprot.a
		

pts: pts.o libprot.a ${SRCDIR}lib/afs/libcmd.a $(LIBS)
	$(CC) ${CFLAGS} -o pts pts.o ${SRCDIR}lib/afs/libcmd.a libprot.a ${LIBS} ${XLIBS}

pts.o: pts.c ${LINCLS} ${SRCDIR}include/afs/cmd.h AFS_component_version_number.c

readgroup: readgroup.o libprot.a $(LIBS)
	$(CC) ${CFLAGS} -o readgroup readgroup.o libprot.a ${LIBS} ${XLIBS}

readgroup.o: readgroup.c ${LINCLS} AFS_component_version_number.c

readpwd: readpwd.o libprot.a $(LIBS)
	$(CC) ${CFLAGS} -o readpwd readpwd.o libprot.a ${LIBS} ${XLIBS}

readpwd.o: readpwd.c ${LINCLS} AFS_component_version_number.c

testpt: testpt.o libprot.a ${SRCDIR}lib/afs/libcmd.a $(LIBS)
	$(CC) ${CFLAGS} -o testpt testpt.o -lm ${SRCDIR}lib/afs/libcmd.a libprot.a $(LIBS) ${XLIBS}

testpt.o: testpt.c ${INCLS} ${SRCDIR}include/afs/cmd.h AFS_component_version_number.c


system: install

kinstall:
	@echo No $@ source here.

ukinstall webinstall: ${UKSRCS} Kptint.h Kptint.cs.c Kptint.xdr.c
	set ${UKERNELDIR}afs; $(MKDIR_IF_NEEDED)
	${INSTALL} Kptint.h ${UKERNELDIR}afsint/ptint.h
	${INSTALL} Kptint.cs.c ${UKERNELDIR}afsint/ptint.cs.c
	${INSTALL} Kptint.xdr.c ${UKERNELDIR}afsint/ptint.xdr.c
	${INSTALL} ${UKSRCS} ${UKERNELDIR}afs

all: ptclient libprot.a pts db_verify readgroup readpwd testpt

${DESTDIR}root.server/usr/afs/bin/ptserver: ptserver
	${INSTALL} $? $@

${DESTDIR}bin/pts ${DESTDIR}root.server/usr/afs/bin/pts: pts
	${INSTALL} $? $@

${DESTDIR}lib/afs/libprot.a: libprot.a
	${INSTALL}   $? $@


install: all ukinstall ${DESTDIR}root.server/usr/afs/bin/ptserver ${DESTDIR}root.server/usr/afs/bin/pts ${DESTDIR}bin/pts ${DESTDIR}lib/afs/libprot.a
	${INSTALL} libprot.a ${DESTDIR}lib/afs/libprot.a
	${INSTALL} ptserver.h ptint.h pterror.h ptclient.h ${DESTDIR}include/afs
	${INSTALL} ptserver.h ${DESTDIR}include/afs/prserver.h
	${INSTALL} ptint.h ${DESTDIR}include/afs/print.h
	${INSTALL} pterror.h ${DESTDIR}include/afs/prerror.h
	${INSTALL} ptclient.h ${DESTDIR}include/afs/prclient.h
	${INSTALL} -f db_verify ${DESTDIR}etc/prdb_check
	echo '#define ERROR_TABLE_BASE_pr ERROR_TABLE_BASE_pt' >> ${DESTDIR}include/afs/prerror.h
