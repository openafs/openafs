#
# (C) COPYRIGHT IBM CORPORATION 1987
# LICENSED MATERIALS - PROPERTY OF IBM
#
SHELL = /bin/sh
COMPONENT=dtest
include ../../config/Makefile.${SYS_NAME}

KERNELDIR = ../libafs/
INSTALL = ${SRCDIR}bin/install

INCDIRS= -I${DESTDIR}include ${XINCLS}
LIBS = ${SRCDIR}lib/afs/libdir.a ${SRCDIR}lib/afs/util.a 
CFLAGS = ${OPTMZ} ${INCDIRS} ${XCFLAGS}

OBJS=test-salvage.o physio.o dtest.o

install:	dtest 

clean:
	rm -f *.o *.a test dtest core

dtest:		dtest.o 
	$(CC) $(LDFLAGS) -o dtest dtest.o $(LIBS)

dtest.o:	dtest.c 
	$(CC) $(CFLAGS) -c dtest.c 

#test-salvage:		test-salvage.o  physio.o
#	$(CC) $(LDFLAGS) -o test-salvage test-salvage.o physio.o $(LIBS)
#
#test-salvage.o:	test-salvage.c  physio.c
#	$(CC) $(CFLAGS) -c test-salvage.c 
#
#physio.o:	physio.c 
#	$(CC) $(CFLAGS) -c physio.c 
