#/* Copyright (C) 1998 Transarc Corporation - All rights reserved */


SHELL = /bin/sh
include ../../config/Makefile.${SYS_NAME}

CC = ${MT_CC}
CFLAGS = ${OPTMZ} ${DBG} -I${SRCDIR}include ${MT_CFLAGS}

ADMINOBJS = afs_clientAdmin.o

LIBOBJS = ${ADMINOBJS}

LIBCLIENTADMIN = ${DESTDIR}/lib/afs/libclientadmin.a

system install: ${LIBCLIENTADMIN}

${LIBCLIENTADMIN}: libclientadmin.a
	${INSTALL} afs_clientAdmin.h ${DESTDIR}include/afs
	${INSTALL} libclientadmin.a ${LIBCLIENTADMIN}

libclientadmin.a: ${LIBOBJS}
	-rm -f $@
	ar rv $@ ${LIBOBJS}
	${RANLIB} $@

afs_clientAdmin.o: afs_clientAdmin.h

clean:
	rm -f *.o libclientadmin*
