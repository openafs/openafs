#
# Copyright (c) 1988 Regents of the University of California.
# All rights reserved.
#
# Redistribution and use in source and binary forms are permitted
# provided that the above copyright notice and this paragraph are
# duplicated in all such forms and that any documentation, advertising
# materials, and other materials related to such redistribution and
# use acknowledge that the software was developed by the University
# of California, Berkeley.  The name of the University may not be
# used to endorse or promote products derived from this software
# without specific prior written permission.  THIS SOFTWARE IS PROVIDED
# ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
# WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND
# FITNESS FOR A PARTICULAR PURPOSE.
#
# @(#)Makefile	5.1 (Berkeley) 9/20/88
#
SHELL=/bin/sh
include ../config/Makefile.${SYS_NAME}

LIBC=	/lib/libc.a
SRCS=	rcp.c
OBJS=   rcp.o ../rsh/rcmd.o ../rsh/herror.o ../inetd/ta-rauth.o
COMPONENT=rcp

INSTALL = ${DESTDIR}bin/install
CFLAGS = ${OPTMZ} -I${DESTDIR}include ${XCFLAGS}
LIBDIR = ${DESTDIR}lib/
AFSLIBS =  ${LIBDIR}afs/libkauth.a ${LIBDIR}libubik.a\
		   ${LIBDIR}afs/libauth.a ${LIBDIR}afs/libsys.a \
		   ${LIBDIR}librx.a ${LIBDIR}liblwp.a ${DESTDIR}lib/librxkad.a\
		   ${DESTDIR}lib/libdes.a ${LIBDIR}afs/libcmd.a \
		   ${LIBDIR}afs/libcom_err.a ${LIBDIR}afs/util.a 
LIBRES = 
LIBS = ${AFSLIBS} ${LIBRES} 
MAN=

include ../config/Makefile.version

all: rcp

rcp.o: AFS_component_version_number.c

system: install

rcp: ${OBJS} ${LIBS}
	set -x; \
	case "${SYS_NAME}" in \
        alpha_osf30 | alpha_osf32 | alpha_osf32c | alpha_dux?? ) \
	   ${CC} -o $@ ${CFLAGS} ${OBJS} ${LIBS} ${XLIBS} -non_shared ;; \
        * ) \
	   ${CC} -o $@ ${CFLAGS} ${OBJS} ${LIBS} ${XLIBS} ;; \
        esac

../rsh/rcmd.o: ../rsh/rcmd.c
	(cd ../rsh ; make rcmd.o SRCDIR=${SRCDIR} DESTDIR=${DESTDIR})

../rsh/herror.o: ../rsh/herror.c
	(cd ../rsh ; make herror.o SRCDIR=${SRCDIR} DESTDIR=${DESTDIR})

../inetd/ta-rauth.o: ../inetd/ta-rauth.c
	(cd ../inetd ; make ta-rauth.o SRCDIR=${SRCDIR} DESTDIR=${DESTDIR})

clean:
clean:
	rm -f ${OBJS} core rcp AFS_component_version_number.c

cleandir: clean
	rm -f ${MAN} tags .depend

depend: ${SRCS}
	mkdep -p ${CFLAGS} ${SRCS}

install: ${MAN} all
	${INSTALL} rcp ${DESTDIR}bin/rcp

lint: ${SRCS}
	lint ${CFLAGS} ${SRCS}

tags: ${SRCS}
	ctags ${SRCS}
