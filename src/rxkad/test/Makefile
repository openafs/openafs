# Copyright 2000, International Business Machines Corporation and others.
# All Rights Reserved.
# 
# This software has been released under the terms of the IBM Public
# License.  For details, see the LICENSE file in the top-level source
# directory or online at http://www.openafs.org/dl/license10.html

BACKDIR=../../../../PARENT/dest/
include ../../config/Makefile.${SYS_NAME}

OPTIMIZE=-g

INCDIRS= -I${DESTDIR}include -I.. -I${BACKDIR}include

HEADERS = ../rxkad.h stress.h stress_internal.h stress_errs.h

LDIR= ${DESTDIR}lib/
ULIBS = ${LDIR}afs/libcmd.a ${LDIR}afs/libcom_err.a ${DESTDIR}lib/afs/util.a

LIBS = ../librxkad.a  ${LDIR}librx.a ${LDIR}libdes.a \
	${LDIR}liblwp.a  ${ULIBS} ${XLIBS}

LIBRPC = ../../librpc/libafsrpc.so
THLIBS= ${LDIR}afs/libcmd.a  ${LDIR}afs/libcom_err.a \
	${ULIBS} ${XLIBS} ${LIBRPC} -lpthread

CFLAGS = ${OPTIMIZE} ${INCDIRS}  ${XCFLAGS}


THFLAGS = -mt -DAFS_PTHREAD_ENV

THRULE = ${CC} ${CFLAGS} ${THFLAGS} -c $?

COMPILE_ET = ${SRCDIR}bin/compile_et

noversion all test system: stress

clean:
	rm -f *.o stress.cs.c stress.ss.c stress.xdr.c stress.h \
		stress_errs.c stress_errs.h stress th_*

stress.ss.o: stress.ss.c
stress.cs.o: stress.cs.c
stress.xdr.o: stress.xdr.c stress.h

th_stress.ss.o: stress.ss.c
	${THRULE} -o th_stress.ss.o

th_stress.cs.o: stress.cs.c
	${THRULE} -o th_stress.cs.o
	
th_stress.xdr.o: stress.xdr.c stress.h
	${THRULE} -o th_stress.xdr.o

stress.ss.c stress.cs.c stress.xdr.c stress.h: stress.rg
	${SRCDIR}bin/rxgen stress.rg

stress_errs.o: stress_errs.c

th_stress_errs.o: stress_errs.c
	${THRULE} -o th_stress_errs.o

stress_errs.c stress_errs.h: stress_errs.et
	rm -f stress_errs.h stress_errs.c; ${COMPILE_ET} stress_errs -h stress_errs

stress.o stress_c.o stress_s.o: ${HEADERS}

th_stress.o: stress.c ${HEADERS}
	${THRULE} -o th_stress.o

th_stress_c.o: stress_c.c ${HEADERS}
	${THRULE} -o th_stress_c.o

th_stress_s.o: stress_s.c ${HEADERS}
	${THRULE} -o th_stress_s.o

stress: stress.o stress.xdr.o stress_c.o stress_s.o stress.cs.o \
		stress.ss.o stress_errs.o 
	$(CC) $(CFLAGS) -o stress stress.o stress.xdr.o stress_c.o \
		stress_s.o stress.cs.o stress.ss.o stress_errs.o ${LIBS}

th_stress: th_stress.o th_stress.xdr.o th_stress_c.o th_stress_s.o \
		th_stress.cs.o th_stress.ss.o stress_errs.o \
		${LIBRPC}
	${CC} ${CFLAGS} ${THFLAGS} -o th_stress th_stress.o th_stress.xdr.o \
		th_stress_c.o th_stress_s.o th_stress.cs.o th_stress.ss.o \
		stress_errs.o ${THLIBS}

