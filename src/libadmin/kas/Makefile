#/* Copyright (C) 1998 Transarc Corporation - All rights reserved */


SHELL = /bin/sh
include ../../config/Makefile.${SYS_NAME}

CC = ${MT_CC}
CFLAGS = ${OPTMZ} ${DBG} -I${SRCDIR}include ${MT_CFLAGS}
CCRULE = ${CC} ${CFLAGS} -c $?

KAUTH= ../../kauth/

ADMINOBJS = afs_kasAdmin.o

KAUTHOBJS = \
	kauth.cs.o \
	kauth.xdr.o \
	kaaux.o

LIBOBJS = ${ADMINOBJS} ${KAUTHOBJS}

LIBKASADMIN = ${DESTDIR}/lib/afs/libkasadmin.a

system install: ${LIBKASADMIN}

${LIBKASADMIN}: libkasadmin.a
	${INSTALL} afs_kasAdmin.h ${DESTDIR}include/afs
	${INSTALL} libkasadmin.a ${LIBKASADMIN}

libkasadmin.a: ${LIBOBJS}
	-rm -f $@
	ar rv $@ ${LIBOBJS}
	${RANLIB} $@

afs_kasAdmin.o: afs_kasAdmin.h

kauth.cs.o: ${KAUTH}/kauth.cs.c
	${CCRULE};

kauth.xdr.o: ${KAUTH}/kauth.xdr.c
	${CCRULE};

kaaux.o: ${KAUTH}/kaaux.c
	${CCRULE};

clean:
	rm -f *.o libkasadmin*
