# Copyright 2000, International Business Machines Corporation and others.
# All Rights Reserved.
# 
# This software has been released under the terms of the IBM Public
# License.  For details, see the LICENSE file in the top-level source
# directory or online at http://www.openafs.org/dl/license10.html

# */

SHELL    =/bin/sh
SRCDIR   =DEST/
DESTDIR  =DEST/
COMPONENT=tbutc

include ../config/Makefile.${SYS_NAME}

CC      =${MT_CC}
CFLAGS  = ${DBG} -w ${MT_CFLAGS}

LDFLAGS = ${DBG} ${XLDFLAGS}

VICED   =../viced
VLSERVER=../vlserver
LWP     =../lwp
LIBACL  =../libacl
UTIL    =../util
DIR     =../dir
VOL     =../vol
FSINT   =../fsint
BUTM    =../butm
BUTC    =../butc
BUCOORD = ../bucoord
VOLSER  = ../volser

# ----------------------------------------------------------------
# The default is to build butc without using the TSM API libraries. 
# To build with TSM support, set XBSA_NOBUILD="". Be sure you have
#  the TSM API version 3.7.1 installed.
# ----------------------------------------------------------------
XBSA_NOBUILD="true"

BUCOORDOBJS=ubik_db_if.o ${BUCOORD}/volstub.o ${BUCOORD}/dlq.o \
	    ${BUCOORD}/status.o ${BUCOORD}/bucoord_errs.o

VOLSERLIBOBJ=${VOLSER}/volint.cs.o  vsprocs.o              ${VOLSER}/vsutils.o \
            ${VOLSER}/lockprocs.o  ${VOLSER}/volint.xdr.o ${VOLSER}/volerr.o \
            ${VOLSER}/volint.ss.o  AFS_component_version_number.o 
VOLSERLIBS=libvolser.a

LWPOBJS    =lock.o

BUTMOBJS   =file_tm.o
BUTMLIBS   =libbutm.a
BUTMINCLS  =${SRCDIR}include/afs/com_err.h ${SRCDIR}include/afs/butm.h
BUTMINCLUDE=-I${SRCDIR}include

BUTCOBJS   =dbentries.o tcprocs.o lwps.o tcmain.o list.o recoverDb.o \
	    tcudbprocs.o dump.o tcstatus.o butc_xbsa.o \
            ${LWPOBJS} ${BUCOORDOBJS}
BUTCINCLS  =${SRCDIR}include/afs/partition.h \
	    ${SRCDIR}include/afs/volume.h \
	    ${SRCDIR}include/afs/vlserver.h \
	    ${SRCDIR}include/rx/rx.h \
	    ${SRCDIR}include/rx/xdr.h \
	    ${SRCDIR}include/afs/keys.h \
	    ${SRCDIR}include/afs/cellconfig.h \
	    ${SRCDIR}include/ubik.h \
	    ${SRCDIR}include/afs/cmd.h \
	    ${SRCDIR}include/afs/butc.h \
	    ${SRCDIR}include/afs/tcdata.h \
	    ${SRCDIR}include/afs/bubasics.h \
	    ${SRCDIR}include/afs/butm.h
BUTCINCLUDE=-I. -I${SRCDIR}include
BUTCLIBS   =${SRCDIR}lib/afs/libbudb.a \
	    ${SRCDIR}lib/afs/libbubasics.a \
	    ${VOLSERLIBS} \
            ${BUTMLIBS} \
	    ${SRCDIR}lib/afs/libvldb.a \
	    ${SRCDIR}/lib/libafsauthent.a \
	    ${SRCDIR}/lib/libafsrpc.a \
            ${SRCDIR}lib/afs/libcmd.a \
	    ${SRCDIR}lib/afs/util.a \
	    ${SRCDIR}lib/afs/libusd.a \
	    ${SRCDIR}lib/afs/libprocmgmt.a \
	    ${SRCDIR}lib/librx.a

include ../config/Makefile.version

#----------------------------------------------- BUTC

butc: pre ${BUTCOBJS} ${BUTCLIBS}
	if [ "${XBSA_NOBUILD}" = "" ]; then xbsaflags="${XBSA_FLAGS}" ; \
	else                                xbsaflags=""; \
	fi; \
	if [ "${XBSA_HDRDIR}" = "" ]; then hdrdir=-I"${XBSADIR}" ; \
	else                               hdrdir=-I"${XBSA_HDRDIR}" ; \
	fi; \
	if [ "$$xbsaflags" = "" ]; then hdrdir="" ; fi; \
	${CC} ${CFLAGS} $$xbsaflags ${BUTCOBJS} ${BUTCLIBS} $$hdrdir ${MTLIBS} ${XLIBS} -o butc

dbentries.o: ${BUTC}/dbentries.c ${BUTCINCLS}
	if [ "${XBSA_NOBUILD}" = "" ]; then xbsaflags="${XBSA_FLAGS}" ; \
	else                                xbsaflags=""; \
	fi; \
	if [ "${XBSA_HDRDIR}" = "" ]; then hdrdir=-I"${XBSADIR}" ; \
	else                               hdrdir=-I"${XBSA_HDRDIR}" ; \
	fi; \
	if [ "$$xbsaflags" = "" ]; then hdrdir="" ; fi; \
	${CC} ${CFLAGS} $$xbsaflags ${BUTCINCLUDE} $$hdrdir -c ${BUTC}/dbentries.c

tcprocs.o: ${BUTC}/tcprocs.c ${BUTCINCLS}
	if [ "${XBSA_NOBUILD}" = "" ]; then xbsaflags="${XBSA_FLAGS}" ; \
	else                                xbsaflags=""; \
	fi; \
	if [ "${XBSA_HDRDIR}" = "" ]; then hdrdir=-I"${XBSADIR}" ; \
	else                               hdrdir=-I"${XBSA_HDRDIR}" ; \
	fi; \
	if [ "$$xbsaflags" = "" ]; then hdrdir="" ; fi; \
	${CC} ${CFLAGS} $$xbsaflags ${BUTCINCLUDE} $$hdrdir -c ${BUTC}/tcprocs.c

lwps.o: ${BUTC}/lwps.c ${BUTCINCLS}
	if [ "${XBSA_NOBUILD}" = "" ]; then xbsaflags="${XBSA_FLAGS}" ; \
	else                                xbsaflags=""; \
	fi; \
	if [ "${XBSA_HDRDIR}" = "" ]; then hdrdir=-I"${XBSADIR}" ; \
	else                               hdrdir=-I"${XBSA_HDRDIR}" ; \
	fi; \
	if [ "$$xbsaflags" = "" ]; then hdrdir="" ; fi; \
	${CC} ${CFLAGS} $$xbsaflags ${BUTCINCLUDE} $$hdrdir -c ${BUTC}/lwps.c

tcmain.o: ${BUTC}/tcmain.c ${BUTCINCLS} AFS_component_version_number.c
	if [ "${XBSA_NOBUILD}" = "" ]; then xbsaflags="${XBSA_FLAGS}" ; \
	else                                xbsaflags=""; \
	fi; \
	if [ "${XBSA_HDRDIR}" = "" ]; then hdrdir=-I"${XBSADIR}" ; \
	else                               hdrdir=-I"${XBSA_HDRDIR}" ; \
	fi; \
	if [ "$$xbsaflags" = "" ]; then hdrdir="" ; fi; \
	${CC} ${CFLAGS} $$xbsaflags ${BUTCINCLUDE} $$hdrdir -c ${BUTC}/tcmain.c

list.o: ${BUTC}/list.c ${BUTCINCLS}
	if [ "${XBSA_NOBUILD}" = "" ]; then xbsaflags="${XBSA_FLAGS}" ; \
	else                                xbsaflags=""; \
	fi; \
	if [ "${XBSA_HDRDIR}" = "" ]; then hdrdir=-I"${XBSADIR}" ; \
	else                               hdrdir=-I"${XBSA_HDRDIR}" ; \
	fi; \
	if [ "$$xbsaflags" = "" ]; then hdrdir="" ; fi; \
	${CC} ${CFLAGS} $$xbsaflags ${BUTCINCLUDE} $$hdrdir -c ${BUTC}/list.c

recoverDb.o: ${BUTC}/recoverDb.c ${BUTCINCLS}
	if [ "${XBSA_NOBUILD}" = "" ]; then xbsaflags="${XBSA_FLAGS}" ; \
	else                                xbsaflags=""; \
	fi; \
	if [ "${XBSA_HDRDIR}" = "" ]; then hdrdir=-I"${XBSADIR}" ; \
	else                               hdrdir=-I"${XBSA_HDRDIR}" ; \
	fi; \
	if [ "$$xbsaflags" = "" ]; then hdrdir="" ; fi; \
	${CC} ${CFLAGS} $$xbsaflags ${BUTCINCLUDE} $$hdrdir -c ${BUTC}/recoverDb.c

tcudbprocs.o: ${BUTC}/tcudbprocs.c ${BUTCINCLS}
	if [ "${XBSA_NOBUILD}" = "" ]; then xbsaflags="${XBSA_FLAGS}" ; \
	else                                xbsaflags=""; \
	fi; \
	if [ "${XBSA_HDRDIR}" = "" ]; then hdrdir=-I"${XBSADIR}" ; \
	else                               hdrdir=-I"${XBSA_HDRDIR}" ; \
	fi; \
	if [ "$$xbsaflags" = "" ]; then hdrdir="" ; fi; \
	${CC} ${CFLAGS} $$xbsaflags ${BUTCINCLUDE} $$hdrdir -c ${BUTC}/tcudbprocs.c

dump.o: ${BUTC}/dump.c ${BUTCINCLS}
	if [ "${XBSA_NOBUILD}" = "" ]; then xbsaflags="${XBSA_FLAGS}" ; \
	else                                xbsaflags=""; \
	fi; \
	if [ "${XBSA_HDRDIR}" = "" ]; then hdrdir=-I"${XBSADIR}" ; \
	else                               hdrdir=-I"${XBSA_HDRDIR}" ; \
	fi; \
	if [ "$$xbsaflags" = "" ]; then hdrdir="" ; fi; \
	${CC} ${CFLAGS} $$xbsaflags ${BUTCINCLUDE} $$hdrdir -c ${BUTC}/dump.c

tcstatus.o: ${BUTC}/tcstatus.c ${BUTCINCLS}
	if [ "${XBSA_NOBUILD}" = "" ]; then xbsaflags="${XBSA_FLAGS}" ; \
	else                                xbsaflags=""; \
	fi; \
	if [ "${XBSA_HDRDIR}" = "" ]; then hdrdir=-I"${XBSADIR}" ; \
	else                               hdrdir=-I"${XBSA_HDRDIR}" ; \
	fi; \
	if [ "$$xbsaflags" = "" ]; then hdrdir="" ; fi; \
	${CC} ${CFLAGS} $$xbsaflags ${BUTCINCLUDE} $$hdrdir -c ${BUTC}/tcstatus.c

butc_xbsa.o: ${BUTC}/butc_xbsa.c ${BUTC}/butc_xbsa.h ${BUTCINCLS}
	if [ "${XBSA_NOBUILD}" = "" ]; then xbsaflags="${XBSA_FLAGS}" ; \
	else                                xbsaflags=""; \
	fi; \
	if [ "${XBSA_HDRDIR}" = "" ]; then hdrdir=-I"${XBSADIR}" ; \
	else                               hdrdir=-I"${XBSA_HDRDIR}" ; \
	fi; \
	if [ "$$xbsaflags" = "" ]; then hdrdir="" ; fi; \
	${CC} ${CFLAGS} $$xbsaflags ${BUTCINCLUDE} $$hdrdir -c ${BUTC}/butc_xbsa.c

#----------------------------------------------- BUCOORD

ubik_db_if.o: ${BUCOORD}/ubik_db_if.c
	if [ "${XBSA_NOBUILD}" = "" ]; then xbsaflags="${XBSA_FLAGS}" ; \
	else                                xbsaflags=""; \
	fi; \
	if [ "${XBSA_HDRDIR}" = "" ]; then hdrdir=-I"${XBSADIR}" ; \
	else                               hdrdir=-I"${XBSA_HDRDIR}" ; \
	fi; \
	if [ "$$xbsaflags" = "" ]; then hdrdir="" ; fi; \
	${CC} ${CFLAGS} $$xbsaflags ${BUTCINCLUDE} $$hdrdir -c ${BUCOORD}/ubik_db_if.c

#----------------------------------------------- VOLSER
# Build the libvolser.a library here. The file compiled
# for is is vsprocs.c. The rest are from the VOLSER dir.

libvolser.a: vsprocs.o ${VOLSERLIBOBJ}
	-rm -f libvolser.a
	ar rv libvolser.a ${VOLSERLIBOBJ}
	$(RANLIB) libvolser.a

vsprocs.o: ${VOLSER}/vsprocs.c
	if [ "${XBSA_NOBUILD}" = "" ]; then xbsaflags="${XBSA_FLAGS}" ; \
	else                                xbsaflags=""; \
	fi; \
	if [ "${XBSA_HDRDIR}" = "" ]; then hdrdir=-I"${XBSADIR}" ; \
	else                               hdrdir=-I"${XBSA_HDRDIR}" ; \
	fi; \
	if [ "$$xbsaflags" = "" ]; then hdrdir="" ; fi; \
	${CC} ${CFLAGS} $$xbsaflags ${BUTCINCLUDE} $$hdrdir -c ${VOLSER}/vsprocs.c

#----------------------------------------------- LWP

lock.o: ${LWP}/lock.c
	if [ "${XBSA_NOBUILD}" = "" ]; then xbsaflags="${XBSA_FLAGS}" ; \
	else                                xbsaflags=""; \
	fi; \
	if [ "${XBSA_HDRDIR}" = "" ]; then hdrdir=-I"${XBSADIR}" ; \
	else                               hdrdir=-I"${XBSA_HDRDIR}" ; \
	fi; \
	if [ "$$xbsaflags" = "" ]; then hdrdir="" ; fi; \
	${CC} ${CFLAGS} $$xbsaflags ${BUTCINCLUDE} $$hdrdir -c ${LWP}/lock.c

#----------------------------------------------- BUTC

file_tm.o: ${BUTM}/file_tm.c ${BUTMINCLS}
	if [ "${XBSA_NOBUILD}" = "" ]; then xbsaflags="${XBSA_FLAGS}" ; \
	else                                xbsaflags=""; \
	fi; \
	if [ "${XBSA_HDRDIR}" = "" ]; then hdrdir=-I"${XBSADIR}" ; \
	else                               hdrdir=-I"${XBSA_HDRDIR}" ; \
	fi; \
	if [ "$$xbsaflags" = "" ]; then hdrdir="" ; fi; \
	${CC} ${CFLAGS} $$xbsaflags ${BUTMINCLUDE} $$hdrdir -c ${BUTM}/file_tm.c

libbutm.a: ${BUTMOBJS} AFS_component_version_number.o
	-rm -f libbutm.a
	${AR} rv libbutm.a $(BUTMOBJS) AFS_component_version_number.o
	${RANLIB} libbutm.a

#-----------------------------------------------
clean:
	rm -f *.o *.a core AFS_component_version_number.c butc

pre:
	if [ "${XBSA_NOBUILD}" = "" ]; then xbsaflags="${XBSA_FLAGS}" ; \
	else                                xbsaflags=""; \
	fi; \
	if [ "$$xbsaflags" = "" ]; then \
		echo 'Will not build with XBSA'; \
	else \
		if [ "${XBSA_HDRDIR}" = "" ]; then hdrdir="${XBSADIR}" ; \
		else                               hdrdir="${XBSA_HDRDIR}" ; \
		fi; \
		if test "$$hdrdir" != "" -a ! -f "$$hdrdir/xbsa.h"; then \
			echo '*** This build of butc cannot support XBSA because'; \
			echo '*** the required header file "'$$hdrdir'/xbsa.h"'; \
			echo '*** does not exist. If you know the pathname to the'; \
			echo '*** header file, then use XBSA_HDRDIR="<path>" as a make'; \
			echo '*** argument.'; \
			echo '*** If you do not want to build an XBSA version of butc,'; \
			echo '*** then use XBSA_NOBUILD="true" as a make argument.'; \
			exit 2; \
		else \
			echo 'Will use XBSA header files from "'$$hdrdir'" (XBSA_HDRDIR)'; \
		fi; \
	fi;

all: butc ${BUTMLIBS}
install install.noversion: all
	${INSTALL} butc ${DESTDIR}etc/butc

noversion: install
system: install
