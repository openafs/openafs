# Copyright 2000, International Business Machines Corporation and others.
# All Rights Reserved.
# 
# This software has been released under the terms of the IBM Public
# License.  For details, see the LICENSE file in the top-level source
# directory or online at http://www.openafs.org/dl/license10.html

# */


SHELL = /bin/sh
COMPONENT=sia
DEBUG = 
#UNRESOLVED = -expect_unresolved \*
UNRESOLVED =
include ../config/Makefile.${SYS_NAME}

LIBS1 = ${DESTDIR}/lib/libubik.a        ${DESTDIR}/lib/afs/libprot.a 
LIBS2 = ${DESTDIR}/lib/librxkad.a       ${DESTDIR}/lib/afs/libaudit.a \
	${DESTDIR}/lib/librx.a          ${DESTDIR}/lib/liblwp.a \
	${DESTDIR}/lib/libdes.a         ${DESTDIR}/lib/afs/libcmd.a \
	${DESTDIR}/lib/afs/libcom_err.a ${DESTDIR}/lib/afs/util.a \
	${DESTDIR}/lib/afs/libsys.a

LIBS  = ${DESTDIR}/lib/afs/libkauth.a ${LIBS1}     ${DESTDIR}/lib/afs/libauth.a ${LIBS2}
KLIBS = ${DESTDIR}/lib/afs/libkauth.krb.a ${LIBS1} ${DESTDIR}/lib/afs/libauth.krb.a ${LIBS2}

all: test-reauth ${DESTDIR}/lib/afs/libafssiad.so ${DESTDIR}/lib/afs/libafssiad.krb.so

CFLAGS = $(DEBUG) -I${SRCDIR}include ${XCFLAGS}

${DESTDIR}/lib/afs/libafssiad.so: libafssiad.so
	${INSTALL} $? $@

${DESTDIR}/lib/afs/libafssiad.krb.so: libafssiad.krb.so
	${INSTALL} $? $@

siad.o: siad.c
	$(CC) $(CFLAGS) -c -g siad.c -o siad.o

siad_krb.o: siad.c
	$(CC) $(CFLAGS) -DAFS_KERBEROS_ENV -c -g siad.c -o siad_krb.o

libafssiad.so: siad.o
	$(LD) $(LDFLAGS) -g -shared -no_archive -o libafssiad.so \
		 ${UNRESOLVED} siad.o ${LIBS} -none -lc

libafssiad.krb.so: siad_krb.o
	$(LD) $(LDFLAGS) -g -shared -no_archive -o libafssiad.krb.so \
		 ${UNRESOLVED} siad_krb.o ${KLIBS} ${LIBS} -none -lc

test-reauth: test-reauth.o
	$(CC) $(CFLAGS) -g -o test-reauth test-reauth.o -lc

install: all



