# Copyright 2000, International Business Machines Corporation and others.
# All Rights Reserved.
# 
# This software has been released under the terms of the IBM Public
# License.  For details, see the LICENSE file in the top-level source
# directory or online at http://www.openafs.org/dl/license10.html

# */


SHELL = /bin/sh
COMPONENT=budb
include ../config/Makefile.${SYS_NAME}

COMPILE_ET = ${SRCDIR}bin/compile_et
RXGEN=${SRCDIR}bin/rxgen

CFLAGS = -g -I${TOP_SRCDIR}/config -I${SRCDIR}include ${XCFLAGS}
LDFLAGS = -g ${XLDFLAGS}
INCLS= \
	${SRCDIR}include/lock.h \
	${SRCDIR}include/ubik.h \
	${SRCDIR}include/lwp.h \
	${SRCDIR}include/des.h \
	${SRCDIR}include/rx/rx.h \
	${SRCDIR}include/rx/xdr.h \
	${SRCDIR}include/rx/rxkad.h \
	${SRCDIR}include/afs/auth.h \
	${SRCDIR}include/afs/cellconfig.h \
	${SRCDIR}include/afs/com_err.h \
	${SRCDIR}include/afs/bubasics.h \
	budb.h budb_client.h database.h

audlib=${SRCDIR}lib/afs/libaudit.a
deslib=${SRCDIR}lib/libdes.a
rxkadlib=${SRCDIR}lib/librxkad.a
kauthlib=${SRCDIR}lib/afs/libkauth.a
authlib=${SRCDIR}lib/afs/libauth.a
cmdlib=${SRCDIR}lib/afs/libcmd.a
utilib=${SRCDIR}lib/afs/util.a
ubiklib=${SRCDIR}lib/libubik.a

# library ordering defined by top level makefile

LIBS= ${SRCDIR}lib/afs/libbubasics.a \
	${audlib}                    \
        ${SRCDIR}lib/afs/libprot.a      \
        ${kauthlib} \
        ${ubiklib} \
        ${authlib} \
        ${rxkadlib} \
        ${SRCDIR}lib/afs/libsys.a \
        ${deslib} \
        ${SRCDIR}lib/librx.a \
        ${SRCDIR}lib/afs/libsys.a \
        ${SRCDIR}lib/liblwp.a \
        ${cmdlib} ${SRCDIR}lib/afs/libcom_err.a \
        ${utilib} 

COMMON_OBJS = database.o db_alloc.o db_dump.o db_hash.o struct_ops.o \
	ol_verify.o

SERVER_OBJS = ${COMMON_OBJS} \
	budb.ss.o budb.xdr.o dbs_dump.o db_lock.o db_text.o procs.o server.o budb_errs.o

LIB_BUDB_OBJS = budb_errs.o budb.cs.o budb.xdr.o struct_ops.o

include ../config/Makefile.version

noversion: install

all: budb_server libbudb.a

clean:
	rm -f *.o *~ budb_errs.[ch] budb.h budb_client.h *.a *.xdr.c \
		*.ss.c *.cs.c core budb_server AFS_component_version_number.c

budb_errs.o: budb_errs.c

budb_errs.c budb_client.h: budb_errs.et budb_client.p.h
	rm -f budb_client.h budb_errs.c; ${COMPILE_ET} budb_errs -h budb_client

budb_errs.h: budb_errs.et
	${COMPILE_ET} budb_errs

database.o: database.c budb_errs.h globals.h ${INCLS}
db_alloc.o: db_alloc.c budb_errs.h ${INCLS}
dbs_dump.o: dbs_dump.c budb_errs.h ${INCLS}
db_lock.o:  db_lock.c budb_errs.h ${INCLS}
db_text.o:  db_text.c budb_errs.h ${INCLS}
db_hash.o: db_hash.c budb_errs.h ${INCLS}
ol_verify.o:	ol_verify.c budb_errs.h ${INCLS}
procs.o: procs.c budb_errs.h globals.h ${INCLS}
server.o: server.c budb_errs.h ${INCLS} AFS_component_version_number.c

# database verification and rebuild

budb_server: $(SERVER_OBJS) ${LIBS}
	${CC} ${LDFLAGS} -o budb_server $(SERVER_OBJS) ${LIBS} ${XLIBS}

budb.xdr.c budb.cs.c budb.ss.c budb.h: budb.rg
	${RXGEN} budb.rg

libbudb.a: ${LIB_BUDB_OBJS} AFS_component_version_number.o
	-rm -f libbudb.a
	${AR} rv libbudb.a $(LIB_BUDB_OBJS) AFS_component_version_number.o
	${RANLIB} libbudb.a

system: install
install: all ${DESTDIR}lib/afs/libbudb.a ${DESTDIR}include/afs/budb.h ${DESTDIR}include/afs/budb_errs.h ${DESTDIR}include/afs/budb_client.h ${DESTDIR}root.server/usr/afs/bin/buserver

${DESTDIR}lib/afs/libbudb.a: libbudb.a
	${INSTALL}  $? $@

${DESTDIR}include/afs/budb.h: budb.h
	${INSTALL}  $? $@

${DESTDIR}include/afs/budb_errs.h: budb_errs.h
	${INSTALL}  $? $@

${DESTDIR}include/afs/budb_client.h: budb_client.h
	${INSTALL}  $? $@

${DESTDIR}root.server/usr/afs/bin/buserver: budb_server
	${INSTALL} -f $? $@
