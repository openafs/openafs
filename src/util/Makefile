# Copyright 2000, International Business Machines Corporation and others.
# All Rights Reserved.
# 
# This software has been released under the terms of the IBM Public
# License.  For details, see the LICENSE file in the top-level source
# directory or online at http://www.openafs.org/dl/license10.html

SHELL = /bin/sh
COMPONENT=util
include ../config/Makefile.${SYS_NAME}

CFLAGS = ${OPTMZ} -I$(SRCDIR)include ${XCFLAGS}
LDFLAGS = ${OPTMZ} ${XLDFLAGS}

objects = assert.o base64.o casestrcpy.o ktime.o volparse.o hostparse.o \
	 hputil.o kreltime.o isathing.o get_krbrlm.o uuid.o serverLog.o \
	 dirpath.o fileutil.o netutils.o flipbase64.o \
	 afs_atomlist.o afs_lhash.o snprintf.o ${REGEX_OBJ}

headers = assert.h potpourri.h itc.h errors.h afsutil.h pthread_glock.h \
	  dirpath.h afs_atomlist.h afs_lhash.h

include ../config/Makefile.version

system noversion: install

all:	util.a sys

${DESTDIR}bin/sys: sys
	${INSTALL} sys ${DESTDIR}bin/sys

install0: ${DESTDIR}include/afs/dirpath.h \
	${DESTDIR}include/afs/pthread_nosigs.h \
	${objects} util.a ${DESTDIR}bin/sys ${someheaders} doc 
	${INSTALL} util.a ${DESTDIR}lib/afs
	${INSTALL} assert.h errors.h vice.h remote.h ktime.h fileutil.h \
		netutils.h packages.h afsutil.h pthread_glock.h \
		afs_atomlist.h afs_lhash.h \
		${DESTDIR}include/afs
	${INSTALL} itc.h potpourri.h ${DESTDIR}include

install: install0 kinstall ukinstall

kinstall:
	${INSTALL} vice.h ../libafs/afs
	${INSTALL} base64.c ../libafs/afs/afs_base64.c
	${INSTALL} uuid.c ../libafs/afs/afs_uuid.c
	${INSTALL} afs_atomlist.c ../libafs/afs
	${INSTALL} afs_atomlist.h ../libafs/afs
	${INSTALL} afs_lhash.c ../libafs/afs
	${INSTALL} afs_lhash.h ../libafs/afs

ukinstall webinstall: install0
	${INSTALL} pthread_glock.h vice.h errors.h afsutil.h dirpath.h fileutil.h netutils.h casestrcpy.c dirpath.c hostparse.c fileutil.c netutils.c ../libuafs/afs
	${INSTALL} uuid.c ../libuafs/afs/afs_uuid.c
	${INSTALL} afs_atomlist.c ../libuafs/afs
	${INSTALL} afs_atomlist.h ../libuafs/afs
	${INSTALL} afs_lhash.c ../libuafs/afs
	${INSTALL} afs_lhash.h ../libuafs/afs

${DESTDIR}include/afs/dirpath.h: dirpath.h
	${INSTALL} dirpath.h  ${DESTDIR}include/afs

${DESTDIR}include/afs/pthread_nosigs.h: pthread_nosigs.h
	${INSTALL} pthread_nosigs.h  ${DESTDIR}include/afs

doc:
	echo no documents in this directory

util.a: ${objects} AFS_component_version_number.o
	rm -f util.a
	$(AR) r util.a ${objects} AFS_component_version_number.o
	$(RANLIB) util.a

volparse.o: volparse.c
	${CC} ${CFLAGS} -c volparse.c

snprintf.o: snprintf.c
	${CC} ${CFLAGS} -c snprintf.c

base64.o: base64.c
	${CC} ${CFLAGS} -c base64.c

hostparse.o: hostparse.c afsutil.h
	${CC} ${CFLAGS} -c hostparse.c

ktime.o: ktime.c
	${CC} ${CFLAGS} -c ktime.c

kreltime.o: kreltime.c
	${CC} ${CFLAGS} -c kreltime.c

get_krbrlm.o: get_krbrlm.c
	${CC} ${CFLAGS} -c get_krbrlm.c

uuid.o: uuid.c
	${CC} ${CFLAGS} -c uuid.c

sys.o: sys.c ${SRCDIR}include/afs/param.h AFS_component_version_number.c
	${CC} ${CFLAGS} -c sys.c

sys: sys.o 
	${CC} ${LDFLAGS} -o sys sys.o

isathing.o: isathing.c
	${CC} ${CFLAGS} -c isathing.c

serverLog.o: serverLog.c
	${CC} ${CFLAGS} -c serverLog.c

dirpath.o: dirpath.c dirpath.h
	${CC} ${CFLAGS} -c dirpath.c

fileutil.o: fileutil.c fileutil.h
	${CC} ${CFLAGS} -c fileutil.c

netutils.o: netutils.c netutils.h
	${CC} ${CFLAGS} -c netutils.c

afs_atomlist.o: afs_atomlist.c afs_atomlist.h
	${CC} ${CFLAGS} -c afs_atomlist.c

afs_lhash.o: afs_lhash.c afs_lhash.h afs_atomlist.h
	${CC} ${CFLAGS} -c afs_lhash.c

clean:
	rm -f ${objects} sys
	rm -f util.a *.o core AFS_component_version_number.c

beancount:
	wc -l *.c *.h

test:
	cd test; $(MAKE)

