# /* Copyright (C) 1993 Transarc Corporation - All rights reserved */

SHELL = 	/bin/sh
COMPONENT=	dauth
include ../config/Makefile.${SYS_NAME}

INSTALL=	${SRCDIR}bin/install
COMPILE_ET = 	${SRCDIR}bin/compile_et
OPTMZ=	-g

CFLAGS = 	${OPTMZ} -I${SRCDIR}include ${XCFLAGS}
LDFLAGS =	${OPTMZ} ${XLDFLAGS}

INCLS=		${SRCDIR}include/ubik.h \
            	${SRCDIR}include/lwp.h \
             	${SRCDIR}include/lock.h  \
             	${SRCDIR}include/rx/rx.h \
             	${SRCDIR}include/rx/xdr.h \
	     	${SRCDIR}include/afs/com_err.h

VERSION =	AFS_component_version_number.o
OBJS =		adkint.cs.o adkint.xdr.o

LIBS = 		${SRCDIR}lib/afs/libkauth.a \
		${SRCDIR}lib/afs/libauth.a \
		${SRCDIR}lib/afs/libcmd.a \
	        ${SRCDIR}lib/afs/libcom_err.a \
		${SRCDIR}lib/libubik.a \
		${SRCDIR}lib/librxkad.a \
		${SRCDIR}lib/libdes.a \
		${SRCDIR}lib/librx.a \
	        ${SRCDIR}lib/liblwp.a \
	       	${SRCDIR}lib/afs/libsys.a \
		${SRCDIR}lib/afs/util.a 

CELL=		`pwd|awk -F/ '{print $$3}'`
USNS=		cellname

include ../config/Makefile.version

ukinstall webinstall:
		echo No $@ source here

noversion:	install

all: 		dlog dpass

clean:
		rm -f adkint.h adkint.cs.c adkint.ss.c adkint.xdr.c *.o dlog

adkint.cs.o:	adkint.cs.c
adkint.xdr.o:	adkint.xdr.c adkint.h
adkint.cs.c	adkint.xdr.c adkint.h: adkint.xg
		${SRCDIR}bin/rxgen adkint.xg

dlog:		dlog.o $(VERSION) $(OBJS) $(LIBS)
		$(CC) ${LDFLAGS} -o dlog dlog.o $(VERSION) $(OBJS) ${LIBS} \
				${XLIBS}

dlog.o: 	dlog.c adkint.h

dpass:		dpass.o $(VERSION) $(LIBS)
		$(CC) ${LDFLAGS} -o dpass dpass.o $(VERSION) $(LIBS) ${XLIBS}

# Test version of dlog just checks ASN.1 date conversion.
dlog_test:	dlog.c $(OBJS) $(LIBS)
		$(CC) $(LDFLAGS) -DDLOG_TEST -o dlog_test dlog.c $(OBJS) \
			$(LIBS) ${XLIBS}
	
system: 	install

${DESTDIR}bin/dlog: dlog
	${INSTALL}   $? $@

${DESTDIR}bin/dpass: dpass
	${INSTALL}   $? $@


install: 	all ${DESTDIR}bin/dlog ${DESTDIR}bin/dpass
