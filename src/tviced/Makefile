# Copyright 2000, International Business Machines Corporation and others.
# All Rights Reserved.
# 
# This software has been released under the terms of the IBM Public
# License.  For details, see the LICENSE file in the top-level source
# directory or online at http://www.openafs.org/dl/license10.html

SHELL=/bin/sh
COMPONENT=tviced
include ../config/Makefile.${SYS_NAME}

CC=${MT_CC}
INCLUDES=-I. -I.. -I${SRCDIR}include -I${SRCDIR}include/afs
CFLAGS=${DBG} ${OPTMZ} -DNINTERFACE ${INCLUDES} ${MT_CFLAGS} -DRXDEBUG
LDFLAGS=${DBG} ${XLDFLAGS}

VICED=../viced
VLSERVER=../vlserver
LWP=../lwp
LIBACL=../libacl
UTIL=../util
DIR=../dir
VOL=../vol
FSINT=../fsint

VICEDOBJS=viced.o afsfileprocs.o host.o physio.o callback.o	

VLSERVEROBJS=vldbint.cs.o vldbint.xdr.o

LWPOBJS=lock.o fasttime.o threadname.o

LIBACLOBJS=aclprocs.o netprocs.o

UTILOBJS=assert.o uuid.o serverLog.o fileutil.o netutils.o dirpath.o \
		volparse.o flipbase64.o

DIROBJS=buffer.o dir.o salvage.o

VOLOBJS= vnode.o volume.o vutil.o partition.o fssync.o purge.o \
	 clone.o devname.o common.o ihandle.o listinodes.o namei_ops.o

FSINTOBJS= afsaux.o afscbint.cs.o afsint.ss.o afsint.xdr.o

objects= ${VICEDOBJS} ${VLSERVEROBJS} ${LWPOBJS} ${LIBACLOBJS} \
	 ${UTILOBJS} ${DIROBJS} ${VOLOBJS} ${FSINTOBJS}

LIBS=	${SRCDIR}lib/libafsauthent.a	\
	${SRCDIR}lib/libafsrpc.a

include ../config/Makefile.version

noversion: install

all: fileserver

CRULE1=${CC} ${CFLAGS} -c $?

viced.o: ${VICED}/viced.c
	${CRULE1}

afsfileprocs.o: ${VICED}/afsfileprocs.c
	${CRULE1}

host.o: ${VICED}/host.c
	${CRULE1}

physio.o: ${VICED}/physio.c
	${CRULE1}

callback.o: ${VICED}/callback.c
	${CRULE1}

assert.o: ${UTIL}/assert.c
	${CRULE1};

uuid.o: ${UTIL}/uuid.c
	${CRULE1};

serverLog.o: ${UTIL}/serverLog.c
	${CRULE1};

fileutil.o: ${UTIL}/fileutil.c
	${CRULE1};

volparse.o: ${UTIL}/volparse.c
	${CRULE1};

flipbase64.o: ${UTIL}/flipbase64.c
	${CRULE1};

netutils.o: ${UTIL}/netutils.c
	${CRULE1};

dirpath.o: ${UTIL}/dirpath.c
	${CRULE1};

lock.o: ${LWP}/lock.c
	${CRULE1};

fasttime.o: ${LWP}/fasttime.c
	${CRULE1};

threadname.o: ${LWP}/threadname.c
	${CRULE1};

aclprocs.o: ${LIBACL}/aclprocs.c
	${CRULE1}

netprocs.o: ${LIBACL}/netprocs.c
	${CRULE1}

vldbint.cs.o: ${VLSERVER}/vldbint.cs.c
	${CRULE1}

vldbint.xdr.o: ${VLSERVER}/vldbint.xdr.c
	${CRULE1}

buffer.o: ${DIR}/buffer.c
	${CRULE1}

dir.o: ${DIR}/dir.c
	${CRULE1}

salvage.o: ${DIR}/salvage.c
	${CRULE1}

vnode.o: ${VOL}/vnode.c
	${CC} -c ${CFLAGS} ${VOL}/vnode.c

volume.o: ${VOL}/volume.c
	${CC} -c ${CFLAGS} ${VOL}/volume.c

vutil.o: ${VOL}/vutil.c
	${CC} -c ${CFLAGS} ${VOL}/vutil.c

partition.o: ${VOL}/partition.c
	${CC} -c ${CFLAGS} ${VOL}/partition.c

fssync.o: ${VOL}/fssync.c
	${CC} -c ${CFLAGS} ${VOL}/fssync.c

purge.o: ${VOL}/purge.c
	${CC} -c ${CFLAGS} ${VOL}/purge.c

clone.o: ${VOL}/clone.c
	${CC} -c ${CFLAGS} ${VOL}/clone.c

nuke.o: ${VOL}/nuke.c
	${CC} -c ${CFLAGS} ${VOL}/nuke.c

devname.o: ${VOL}/devname.c
	${CC} -c ${CFLAGS} ${VOL}/devname.c

common.o: ${VOL}/common.c
	${CC} -c ${CFLAGS} ${VOL}/common.c

listinodes.o: ${VOL}/listinodes.c
	${CC} -c ${CFLAGS} ${VOL}/listinodes.c

ihandle.o: ${VOL}/ihandle.c
	${CC} -c ${CFLAGS} ${VOL}/ihandle.c

namei_ops.o: ${VOL}/namei_ops.c
	${CC} -c ${CFLAGS} ${VOL}/namei_ops.c

afsaux.o: ${FSINT}/afsaux.c
	${CC} -c ${CFLAGS} ${FSINT}/afsaux.c

afscbint.cs.o: ${FSINT}/afscbint.cs.c
	${CC} -c ${CFLAGS} ${FSINT}/afscbint.cs.c

afscbint.ss.o: ${FSINT}/afscbint.ss.c
	${CC} -c ${CFLAGS} ${FSINT}/afscbint.ss.c

afsint.cs.o: ${FSINT}/afsint.cs.c
	${CC} -c ${CFLAGS} ${FSINT}/afsint.cs.c

afsint.ss.o: ${FSINT}/afsint.ss.c
	${CC} -c ${CFLAGS} ${FSINT}/afsint.ss.c

afsint.xdr.o: ${FSINT}/afsint.xdr.c
	${CC} -c ${CFLAGS} ${FSINT}/afsint.xdr.c

fileserver: callback_clean1 viced.o ${objects} ${LIBS}
	case ${SYS_NAME} in \
	*linux*) \
	    ${CC} ${LDFLAGS} -o fileserver ${objects} ${LIBS} \
	    ${MTLIBS} ${XLIBS} ;; \
	*) \
	    ${CC} ${LDFLAGS} -o fileserver ${objects} ${LIBS} \
	    ${MTLIBS} ${XLIBS} ;; \
	esac;

${DESTDIR}root.server/usr/afs/bin/fileserver : fileserver
	${INSTALL} -ns fileserver ${DESTDIR}root.server/usr/afs/bin/fileserver

install: all  ${DESTDIR}root.server/usr/afs/bin/fileserver

system: install

callback_clean1 callback_clean2: 
	case ${SYS_NAME} in \
	ncrx86_*) \
		${RM}  -f callback.o;\
		echo ${RM} callback.o;;\
	esac;

lint:

clean:
	rm -f *.o fileserver core AFS_component_version_number.c

beancount:
	wc -l *.c *.h
