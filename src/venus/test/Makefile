# Copyright 2000, International Business Machines Corporation and others.
# All Rights Reserved.
# 
# This software has been released under the terms of the IBM Public
# License.  For details, see the LICENSE file in the top-level source
# directory or online at http://www.openafs.org/dl/license10.html

include ../../config/Makefile.${SYS_NAME}

# SRCDIR=DEST/
# DESTDIR=DEST/
OPTIMIZE=-O

INCDIRS= -I${TOP_SRCDIR}/config -I${DESTDIR}include -I..
LDIRS= -L${DESTDIR}lib -L${DESTDIR}lib/afs -L..
utilib=${DESTDIR}lib/afs/util.a
LIBS= -lsys -lubik -lvldb -lauth -lrxkad -ldes -lcom_err -lcmd -lrx -llwp ${utilib}

CFLAGS = ${OPTIMIZE} ${INCDIRS} ${XCFLAGS}

install all system test:  fulltest owntest idtest getinitparams

fulltest: fulltest.o
	$(CC) $(CFLAGS) -o $@ $@.o $(LDIRS) $(LIBS)
owntest: owntest.o
	$(CC) $(CFLAGS) -o $@ $@.o $(LDIRS) $(LIBS)
idtest: idtest.o
	$(CC) $(CFLAGS) -o $@ $@.o $(LDIRS) $(LIBS)
getinitparams: getinitparams.o
	$(CC) $(CFLAGS) -o $@ $@.o $(LDIRS) $(LIBS)

clean:
	rm -f *.o fulltest owntest idtest getinitparams

