# Copyright 2000, International Business Machines Corporation and others.
# All Rights Reserved.
# 
# This software has been released under the terms of the IBM Public
# License.  For details, see the LICENSE file in the top-level source
# directory or online at http://www.openafs.org/dl/license10.html

COMPONENT=log
include ../config/Makefile.${SYS_NAME}

INCDIRS= -I${TOP_SRCDIR}/config -I${DESTDIR}include -I${DESTDIR}include/afs 
utilib=${DESTDIR}lib/afs/util.a

CFLAGS = ${OPTMZ} ${INCDIRS} ${XCFLAGS} ${DBUG}

PROGRAMS=	unlog tokens tokens.krb kseal

INCLS=${SRCDIR}include/afs/auth.h ${SRCDIR}include/rx/rx.h \
${SRCDIR}include/rx/xdr.h ${SRCDIR}include/des.h ${SRCDIR}include/rx/rxkad.h \
${SRCDIR}include/afs/cellconfig.h

LIBRARIES=	${SRCDIR}lib/afs/libauth.a \
		${SRCDIR}lib/librxkad.a \
		${SRCDIR}lib/libdes.a \
		${SRCDIR}lib/afs/libsys.a ${SRCDIR}lib/librx.a \
		${SRCDIR}lib/afs/libsys.a \
	        ${SRCDIR}lib/liblwp.a ${SRCDIR}lib/afs/libcmd.a \
	        ${SRCDIR}lib/afs/util.a
KLIBRARIES=	${SRCDIR}lib/afs/libauth.krb.a \
		${SRCDIR}lib/librxkad.a \
		${SRCDIR}lib/libdes.a \
		${SRCDIR}lib/afs/libsys.a ${SRCDIR}lib/librx.a \
		${SRCDIR}lib/afs/libsys.a \
	        ${SRCDIR}lib/liblwp.a ${SRCDIR}lib/afs/libcmd.a \
	        ${SRCDIR}lib/afs/util.a

include ../config/Makefile.version

# top-level options

all:	${PROGRAMS}

install:	all
	${INSTALL} -s unlog tokens tokens.krb ${DESTDIR}bin
	${INSTALL} -s tokens tokens.krb ${DESTDIR}root.server/usr/afs/bin
	${INSTALL} -s kseal ${DESTDIR}etc

lint:	log.c passwd.c unlog.c testlog.c
	lint  ${INCDIRS} log.c 
	lint  ${INCDIRS} unlog.c 
	lint  ${INCDIRS} passwd.c

clean:	
	rm -f *.o log passwd unlog tokens kseal core AFS_component_version_number.c tokens.krb

beancount:
	wc -l log.c passwd.c unlog.c 

# programs

kseal: kseal.o ${LIBRARIES}
	${CC} -o kseal kseal.o ${LIBRARIES} ${XLIBS} ${CFLAGS}

log: log.o ${LIBRARIES}
	${CC} -o log log.o ${LIBRARIES} ${XLIBS} ${CFLAGS}

unlog: unlog.o ${LIBRARIES}
	${CC} -o unlog unlog.o ${LIBRARIES} ${XLIBS} ${CFLAGS}

passwd:	passwd.o ${LIBRARIES}
	${CC} -o passwd passwd.o ${LIBRARIES} ${XLIBS} ${CFLAGS}

tokens: tokens.o ${LIBRARIES}
	${CC} -o tokens tokens.o ${LIBRARIES} ${XLIBS} ${CFLAGS}

tokens.krb: tokens.o ${KLIBRARIES}
	${CC} -o tokens.krb tokens.o ${KLIBRARIES} ${XLIBS} ${CFLAGS}

# objects

kseal.o:		kseal.c ${INCLS} AFS_component_version_number.c
unlog.o:		unlog.c AFS_component_version_number.c
tokens.o:		tokens.c ${SRCDIR}include/rx/xdr.h AFS_component_version_number.c
#log.o:		log.c ${SRCDIR}include/afs/comauth.h ${SRCDIR}include/r/xdr.h
#passwd.o:	passwd.c ${SRCDIR}include/afs/comauth.h ${SRCDIR}include/r/xdr.h ${SRCDIR}include/r/r.h

test:
	cd test; $(MAKE)
