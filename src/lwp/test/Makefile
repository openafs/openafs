# Copyright 2000, International Business Machines Corporation and others.
# All Rights Reserved.
# 
# This software has been released under the terms of the IBM Public
# License.  For details, see the LICENSE file in the top-level source
# directory or online at http://www.openafs.org/dl/license10.html

SRCDIR=DEST/
DESTDIR=DEST/
OPTIMIZE= -g

INCDIRS= -I.. -I${DESTDIR}include 
LIBS= ../liblwp.a

include ../../config/Makefile.${SYS_NAME}

CFLAGS = ${OPTIMIZE} ${INCDIRS} ${LDIRS} ${XCFLAGS}

system noversion: test

test:		test.c liblwp.a
		${CC} -p ${CFLAGS} test.c -o test ${LIBS}

testlwp.o	: testlwp.c
		${CC} -c -p ${CFLAGS} testlwp.c ${LIBS}


selclient.o: selclient.c seltest.h
	${CC} ${CFLAGS} -c -g selclient.c

selclient: selclient.o selsubs.o ${LIBS}
	${CC} ${CFLAGS} -g -o selclient selclient.o selsubs.o ${LIBS}

selserver.o: selserver.c seltest.h
	${CC} ${CFLAGS} -c -g selserver.c

selserver: selserver.o selsubs.o ${LIBS}
	${CC} ${CFLAGS} -g -o selserver selserver.o selsubs.o ${LIBS}

selsubs.o: selsubs.c seltest.h
	${CC} ${CFLAGS} -g -c selsubs.c


test_key: test_key.o
	${CC} ${CFLAGS} -g -o test_key test_key.o \
		../liblwp.a DEST/lib/afs/util.a



clean:
	-rm -f *.o *.a test testlwp selclient selserver core
