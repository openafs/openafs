# Copyright 2000, International Business Machines Corporation and others.
# All Rights Reserved.
# 
# This software has been released under the terms of the IBM Public
# License.  For details, see the LICENSE file in the top-level source
# directory or online at http://www.openafs.org/dl/license10.html

SHELL = /bin/sh
COMPONENT=bucoord
include ../config/Makefile.${SYS_NAME}

COMPILE_ET = ${SRCDIR}bin/compile_et

CFLAGS= ${DBUG} -w -I${SRCDIR}include -I${SRCDIR}include/afs ${XCFLAGS}

#LIBS=${SRCDIR}lib/afs/libcmd.a ${SRCDIR}lib/afs/libbubasics.a \
#${SRCDIR}lib/afs/libcom_err.a ${SRCDIR}lib/afs/util.a \
#$(SRCDIR)lib/librx.a $(SRCDIR)lib/liblwp.a

LIBS=${SRCDIR}lib/afs/libbudb.a ${SRCDIR}lib/afs/libbubasics.a \
        ${SRCDIR}lib/afs/libbutm.a ${SRCDIR}lib/afs/libvolser.a \
        ${SRCDIR}lib/afs/libvldb.a  ${SRCDIR}lib/afs/vlib.a \
        ${SRCDIR}lib/afs/libkauth.a ${SRCDIR}lib/libubik.a \
        ${SRCDIR}lib/afs/libauth.a  ${SRCDIR}lib/librxkad.a \
        ${SRCDIR}lib/afs/libsys.a   ${SRCDIR}lib/libdes.a \
        ${SRCDIR}lib/librx.a  ${SRCDIR}lib/afs/libsys.a \
        ${SRCDIR}lib/liblwp.a ${SRCDIR}lib/afs/libcmd.a \
        ${SRCDIR}lib/afs/libcom_err.a ${SRCDIR}lib/afs/util.a 

INCLS=bc.h ${SRCDIR}include/afs/butc.h
RXGEN=${SRCDIR}bin/rxgen

include ../config/Makefile.version

noversion: install

all: backup libbxdb.a 

libbxdb.a: dsstub.o ubik_db_if.o status.o dlq.o bucoord_errs.o volstub.o AFS_component_version_number.o
	-rm -f libbxdb.a
	${AR} r libbxdb.a dsstub.o ubik_db_if.o status.o dlq.o \
		bucoord_errs.o volstub.o AFS_component_version_number.o
	${RANLIB} libbxdb.a

ttest: ttest.o ${SRCDIR}lib/librx.a ${SRCDIR}lib/liblwp.a libbxdb.a
	${CC} ${CFLAGS} -o ttest ttest.o libbxdb.a \
		${SRCDIR}lib/afs/libbubasics.a \
${SRCDIR}lib/librx.a ${SRCDIR}lib/liblwp.a

ttest.o: AFS_component_version_number.c

btest: btest.o ${SRCDIR}lib/afs/libbubasics.a ${SRCDIR}lib/librx.a \
		${SRCDIR}lib/liblwp.a
	${CC} ${CFLAGS} -o btest btest.o ${SRCDIR}lib/afs/libbubasics.a \
		${SRCDIR}lib/librx.a ${SRCDIR}lib/liblwp.a

btest.o: AFS_component_version_number.c

BACKSRCS = main.c config.c dsstub.c volstub.c commands.c regex.c server.c \
	dsvs.c dump.c restore.c ubik_db_if.c 				  \
	dump_sched.c vol_sets.c tape_hosts.c				  \
	bc_status.c status.c dlq.c

BACKOBJS = main.o config.o dsstub.o volstub.o commands.o regex.o server.o \
	dsvs.o dump.o restore.o ubik_db_if.o dump_sched.o vol_sets.o	\
	tape_hosts.o bucoord_errs.o bc_status.o status.o dlq.o

main.o: AFS_component_version_number.c

$(BACKOBJS): $(INCLS)

backup:  $(BACKOBJS) ${LIBS}
	${CC} ${CFLAGS} -o backup $(BACKOBJS) ${LIBS} ${XLIBS}

bucoord_errs.c bc.h:	bucoord_errs.et bc.p.h
	rm -f bc.h bucoord_errs.c
	${COMPILE_ET} bucoord_errs -h bc

system: install

install: all
	${INSTALL} libbxdb.a ${DESTDIR}lib/afs
	${INSTALL} backup ${DESTDIR}etc

clean:
	rm -f *.o backup convert *.ss.c *.cs.c *.xdr.c *.a core btest ttest AFS_component_version_number.c

lint:
	lint -IDEST/include -IDEST/include/afs *.h *.c

cxref:
	cxref $(CFLAGS) $(BACKSRCS) > cxref.out
