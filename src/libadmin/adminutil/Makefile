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
COMPILE_ET = ${SRCDIR}bin/compile_et
RXKAD = ../../rxkad
AUTH = ../../auth
BOZO = ../../bozo
CMD = ../../cmd
KAUTH = ../../kauth
PTSERVER = ../../ptserver
UBIK = ../../ubik
VLSERVER = ../../vlserver
VOLSER = ../../volser

afs_AdminBosErrors.c ${DESTDIR}/include/afs/afs_AdminBosErrors.h: afs_AdminBosErrors.et
	${INSTALL} afs_AdminErrors.h ${DESTDIR}include/afs
	rm -f afs_AdminBosErrors.c afs_AdminBosErrors.h
	${COMPILE_ET} afs_AdminBosErrors.et -h afs_AdminBosErrors
	cp afs_AdminBosErrors.h ${DESTDIR}/include/afs/afs_AdminBosErrors.h

afs_AdminCfgErrors.c ${DESTDIR}/include/afs/afs_AdminCfgErrors.h: afs_AdminCfgErrors.et
	rm -f afs_AdminCfgErrors.c afs_AdminCfgErrors.h
	${COMPILE_ET} afs_AdminCfgErrors.et -h afs_AdminCfgErrors
	cp afs_AdminCfgErrors.h ${DESTDIR}/include/afs/afs_AdminCfgErrors.h
 
afs_AdminClientErrors.c ${DESTDIR}/include/afs/afs_AdminClientErrors.h: afs_AdminClientErrors.et
	rm -f afs_AdminClientErrors.c afs_AdminClientErrors.h
	${COMPILE_ET} afs_AdminClientErrors.et -h afs_AdminClientErrors
	cp afs_AdminClientErrors.h ${DESTDIR}/include/afs/afs_AdminClientErrors.h
 
afs_AdminMiscErrors.c ${DESTDIR}/include/afs/afs_AdminMiscErrors.h: afs_AdminMiscErrors.et
	rm -f afs_AdminMiscErrors.c afs_AdminMiscErrors.h
	${COMPILE_ET} afs_AdminMiscErrors.et -h afs_AdminMiscErrors
	cp afs_AdminMiscErrors.h ${DESTDIR}/include/afs/afs_AdminMiscErrors.h
 
afs_AdminCommonErrors.c ${DESTDIR}/include/afs/afs_AdminCommonErrors.h: afs_AdminCommonErrors.et
	rm -f afs_AdminCommonErrors.c afs_AdminCommonErrors.h
	${COMPILE_ET} afs_AdminCommonErrors.et -h afs_AdminCommonErrors
	cp afs_AdminCommonErrors.h ${DESTDIR}/include/afs/afs_AdminCommonErrors.h
 
afs_AdminKasErrors.c ${DESTDIR}/include/afs/afs_AdminKasErrors.h: afs_AdminKasErrors.et
	rm -f afs_AdminKasErrors.c afs_AdminKasErrors.h
	${COMPILE_ET} afs_AdminKasErrors.et -h afs_AdminKasErrors
	cp afs_AdminKasErrors.h ${DESTDIR}/include/afs/afs_AdminKasErrors.h
 
afs_AdminPtsErrors.c ${DESTDIR}/include/afs/afs_AdminPtsErrors.h: afs_AdminPtsErrors.et
	rm -f afs_AdminPtsErrors.c afs_AdminPtsErrors.h
	${COMPILE_ET} afs_AdminPtsErrors.et -h afs_AdminPtsErrors
	cp afs_AdminPtsErrors.h ${DESTDIR}/include/afs/afs_AdminPtsErrors.h
 
afs_AdminUtilErrors.c ${DESTDIR}/include/afs/afs_AdminUtilErrors.h: afs_AdminUtilErrors.et
	rm -f afs_AdminUtilErrors.c afs_AdminUtilErrors.h
	${COMPILE_ET} afs_AdminUtilErrors.et -h afs_AdminUtilErrors
	cp afs_AdminUtilErrors.h ${DESTDIR}/include/afs/afs_AdminUtilErrors.h
 
afs_AdminVosErrors.c ${DESTDIR}/include/afs/afs_AdminVosErrors.h: afs_AdminVosErrors.et
	rm -f afs_AdminVosErrors.c afs_AdminVosErrors.h
	${COMPILE_ET} afs_AdminVosErrors.et -h afs_AdminVosErrors
	cp afs_AdminVosErrors.h ${DESTDIR}/include/afs/afs_AdminVosErrors.h

ERROROBJS = \
	rxkad_errs.o \
	boserr.o \
	kaerrors.o \
	ktc_errors.o \
	cmd_errors.o \
	pterror.o \
	uerrors.o \
	vl_errors.o \
	acfg_errors.o \
	volerr.o

ADMINOBJS = \
	afs_AdminBosErrors.o \
	afs_AdminCfgErrors.o \
	afs_AdminClientErrors.o \
	afs_AdminCommonErrors.o \
	afs_AdminKasErrors.o \
	afs_AdminMiscErrors.o \
	afs_AdminPtsErrors.o \
	afs_AdminUtilErrors.o \
	afs_AdminVosErrors.o \
	afs_utilAdmin.o

LIBOBJS = ${ADMINOBJS} ${ERROROBJS}

LIBADMINUTIL = ${DESTDIR}/lib/afs/libafsadminutil.a

system install: ${LIBADMINUTIL}

${LIBADMINUTIL}: libafsadminutil.a
	${INSTALL} afs_utilAdmin.h ${DESTDIR}include/afs
	${INSTALL} libafsadminutil.a ${LIBADMINUTIL}

libafsadminutil.a: ${LIBOBJS}
	-rm -f $@
	ar rv $@ ${LIBOBJS}
	${RANLIB} $@

afs_utilAdmin.o: afs_utilAdmin.h

ktc_errors.o: ${AUTH}/ktc_errors.c
	${CCRULE}
	
acfg_errors.o: ${AUTH}/acfg_errors.c
	${CCRULE}
	
boserr.o: ${BOZO}/boserr.c
	${CCRULE}

cmd_errors.o: ${CMD}/cmd_errors.c
	${CCRULE}

kaerrors.o: ${KAUTH}/kaerrors.c
	${CCRULE}

pterror.o: ${PTSERVER}/pterror.c
	${CCRULE}

rxkad_errs.o: ${RXKAD}/rxkad_errs.c
	${CCRULE}

uerrors.o: ${UBIK}/uerrors.c
	${CCRULE}

vl_errors.o: ${VLSERVER}/vl_errors.c
	${CCRULE}

volerr.o: ${VOLSER}/volerr.c
	${CCRULE}

clean:
	rm -f *.o libafsadminutil*
	rm -f afs_AdminBosErrors.c afs_AdminBosErrors.h
	rm -f afs_AdminCfgErrors.c afs_AdminCfgErrors.h
	rm -f afs_AdminClientErrors.c afs_AdminClientErrors.h
	rm -f afs_AdminCommonErrors.c afs_AdminCommonErrors.h
	rm -f afs_AdminKasErrors.c afs_AdminKasErrors.h
	rm -f afs_AdminMiscErrors.c afs_AdminMiscErrors.h
	rm -f afs_AdminPtsErrors.c afs_AdminPtsErrors.h
	rm -f afs_AdminUtilErrors.c afs_AdminUtilErrors.h
	rm -f afs_AdminVosErrors.c afs_AdminVosErrors.h
