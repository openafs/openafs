# Copyright 2000, International Business Machines Corporation and others.
# All Rights Reserved.
# 
# This software has been released under the terms of the IBM Public
# License.  For details, see the LICENSE file in the top-level source
# directory or online at http://www.openafs.org/dl/license10.html

SHELL = /bin/sh
include ../../config/Makefile.${SYS_NAME}

CC = ${MT_CC}
CFLAGS = ${OPTMZ} ${DBG} -I${TOP_SRCDIR}/config -I${SRCDIR}include ${MT_CFLAGS}
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
