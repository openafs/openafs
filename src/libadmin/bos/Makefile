#/* Copyright (C) 1998 Transarc Corporation - All rights reserved */


SHELL = /bin/sh
include ../../config/Makefile.${SYS_NAME}

CC = ${MT_CC}
CFLAGS = ${OPTMZ} ${DBG} -I${SRCDIR}include ${MT_CFLAGS} 
CCRULE = ${CC} ${CFLAGS} -c $?

BOZO = ../../bozo/

ADMINOBJS = afs_bosAdmin.o

BOZOOBJS = bosint.xdr.o bosint.cs.o

LIBOBJS = ${ADMINOBJS} ${BOZOOBJS}

LIBBOSADMIN = ${DESTDIR}/lib/afs/libbosadmin.a

system install: ${LIBBOSADMIN}

${LIBBOSADMIN}: libbosadmin.a
	${INSTALL} afs_bosAdmin.h ${DESTDIR}include/afs
	${INSTALL} libbosadmin.a ${LIBBOSADMIN}

libbosadmin.a: ${LIBOBJS}
	-rm -f $@
	ar rv $@ ${LIBOBJS}
	${RANLIB} $@

afs_bosAdmin.o: afs_bosAdmin.h

bosint.xdr.o: ${BOZO}/bosint.xdr.c
	${CCRULE}

bosint.cs.o: ${BOZO}/bosint.cs.c
	${CCRULE}

clean:
	rm -f *.o libbosadmin*
