
#/* Copyright (C) 1998, 1989 Transarc Corporation - All rights reserved */
#
# (C) COPYRIGHT IBM CORPORATION 1988
# LICENSED MATERIALS - PROPERTY OF IBM
#

SHELL = /bin/sh
include ../../config/Makefile.${SYS_NAME}
#include ../../config/Makefile.version

INSTALL=${SRCDIR}bin/install
RXGEN=${SRCDIR}bin/rxgen
COMPILE_ET = ${SRCDIR}bin/compile_et

INCDIRS= -I${DESTDIR}include  -I.. -I${BACKDIR}include
CFLAGS = $(DBUG) -I${SRCDIR}include ${INCDIRS} ${XCFLAGS}
LIBUSD=DEST/lib/afs/libusd.a

#####################################################################
# include files
INCLS=${SRCDIR}include/afs/param.h ${SRCDIR}include/afs/usd.h


# build usd_test
usd_test: usd_test.o ${LIBUSD} $(INCLS)
	$(CC) ${CFLAGS} -o usd_test usd_test.o ${LIBUSD} ${XLIBS}

usd_test.o: usd_test.c

clean:
	-rm  -f *.o usd_test \
	 core  AFS_component_version_number.c

#######################################################################

test tests: usd_test

kinstall:
	@echo No kernel source here.




