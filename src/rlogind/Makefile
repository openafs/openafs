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
# @(#)Makefile	5.3 (Berkeley) 9/20/88
#

SHELL=/bin/sh
COMPONENT=rlogind
include ../config/Makefile.${SYS_NAME}

LIBC=	/lib/libc.a
SRCS=	rlogind.c ../rsh/rcmd.c ../rsh/herror.c ../inetd/ta-rauth.c
OBJS=	rlogind.o ../rsh/rcmd.o ../rsh/herror.o ../inetd/ta-rauth.o
MAN=
LIBDIR = ${DESTDIR}lib/
AFSLIBS =  ${LIBDIR}afs/libkauth.a ${LIBDIR}afs/libprot.a ${LIBDIR}libubik.a\
		   ${LIBDIR}afs/libauth.a ${DESTDIR}lib/librxkad.a ${LIBDIR}afs/libsys.a \
		   ${LIBDIR}librx.a ${LIBDIR}liblwp.a \
	 	   ${LIBDIR}afs/libsys.a \
		   ${DESTDIR}lib/librxkad.a ${DESTDIR}lib/libdes.a \
	           ${LIBDIR}afs/libcmd.a ${LIBDIR}afs/libcom_err.a ${LIBDIR}afs/util.a ${LIBDIR}afs/libaudit.a
RES = 
LIBS = ../login/libutil.a ${RES} ${AFSLIBS}
ALIBS= ${AFSLIBS} 
CFLAGS= ${OPTMZ} -I${TOP_SRCDIR}/config -I${DESTDIR}include ${XCFLAGS}

include ../config/Makefile.version

all: rlogind rexecd

system: install

rlogind: ${OBJS} ${LIBS} ../login/libutil.a
	case ${SYS_NAME} in \
	rs_aix*) \
		${CC} -o $@ ${CFLAGS} ${OBJS} ${LIBS} ${XLIBS} -ls ;; \
	hp_ux110) \
		${CC} -o $@ ${CFLAGS} ${OBJS} ${LIBS} ${XLIBS} -lsec ;; \
	*) ${CC} -o $@ ${CFLAGS} ${OBJS} ${LIBS} ${XLIBS} ;;\
	esac ;

rlogind.o: rlogind.c AFS_component_version_number.c

rexecd.o: rexecd.c AFS_component_version_number.c

rexecd: rexecd.o ${ALIBS}
	case ${SYS_NAME} in \
	rs_aix*) \
		${CC} -o $@ ${CFLAGS} rexecd.o ${ALIBS} ${XLIBS} -ls ;; \
	*) ${CC} -o $@ ${CFLAGS} rexecd.o ${ALIBS} ${XLIBS} ;;\
	esac ;


../login/libutil.a : ../login/util_login.c ../login/util_logout.c ../login/util_logwtmp.c
	( cd ../login ; $(MAKE) libutil.a )

../rsh/rcmd.o : ../rsh/rcmd.c
	(cp AFS_component_version_number.c ../rsh/AFS_component_version_number.c ;  cd ../rsh ; $(MAKE) rsh.o )

../rsh/herror.o : ../rsh/herror.c
	( cd ../rsh ; $(MAKE) herror.o )

clean:
	rm -f ${OBJS} core rlogind AFS_component_version_number.c

cleandir: clean
	rm -f ${MAN} tags .depend

depend: ${SRCS}
	mkdep -p ${CFLAGS} ${SRCS}

install: ${MAN} all
	${INSTALL} -s -m 755 rlogind ${DESTDIR}etc/rlogind
	${INSTALL} -s -m 755 rexecd ${DESTDIR}etc/rexecd

lint: ${SRCS}
	lint ${CFLAGS} ${SRCS}

tags: ${SRCS}
	ctags ${SRCS}
