#/* Copyright (C) 1998 Transarc Corporation - All rights reserved */


SHELL = /bin/sh
include ../../config/Makefile.${SYS_NAME}

CFLAGS = ${OPTMZ} ${DBG} -I${SRCDIR}include ${MT_CFLAGS}
CCRULE = ${CC} ${CFLAGS} -c $?

UBIKOBJS=\
	ubik_int.cs.o \
	ubik_int.xdr.o

CFGOBJS=\
	cfgclient.o \
	cfgdb.o \
	cfghost.o \
	cfgservers.o \
	cfginternal.o

LIBOBJS =\
	${UBIKOBJS} \
	${CFGOBJS}


LIBCFGADMIN = ${DESTDIR}/lib/afs/libcfgadmin.a

system install: ${LIBCFGADMIN}

${LIBCFGADMIN}: libcfgadmin.a
	${INSTALL} afs_cfgAdmin.h ${DESTDIR}include/afs
	${INSTALL} libcfgadmin.a ${LIBCFGADMIN}

libcfgadmin.a: ${LIBOBJS}
	-rm -f $@
	ar rv $@ ${LIBOBJS}
	${RANLIB} $@

UBIKDIR=../../ubik

ubik_int.cs.o: ${UBIKDIR}/ubik_int.cs.c
	${CCRULE}

ubik_int.xdr.o: ${UBIKDIR}/ubik_int.xdr.c
	${CCRULE}

${CFGOBJS}: afs_cfgAdmin.h

clean:
	rm -f *.o libcfgadmin*
