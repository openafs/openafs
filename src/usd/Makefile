#/* Copyright (C) 1998, 1989 Transarc Corporation - All rights reserved */
#
# (C) COPYRIGHT IBM CORPORATION 1988
# LICENSED MATERIALS - PROPERTY OF IBM
#

SHELL = /bin/sh
COMPONENT=ptserver
include ../config/Makefile.${SYS_NAME}

INSTALL=${SRCDIR}bin/install
RXGEN=${SRCDIR}bin/rxgen
COMPILE_ET = ${SRCDIR}bin/compile_et

CFLAGS = $(DBG) -I${SRCDIR}include ${XCFLAGS}

#####################################################################
# include files
INCLS=${SRCDIR}include/afs/param.h \
	usd.h

LINCLS=usd.h

#####################################################################
include ../config/Makefile.version

clean:
	$(RM) -f *.o libusd.a   \
	 core  AFS_component_version_number.c

########################################################################
# build libusd.a
LIBOBJS=usd_file.o

libusd.a: ${LIBOBJS} AFS_component_version_number.o
	$(RM) -f libusd.a
	$(AR) r libusd.a ${LIBOBJS} AFS_component_version_number.o
	$(RANLIB) libusd.a

usd_file.o: usd_file.c $(LINCLS)

#######################################################################
system: install

kinstall:
	@echo No kernel source here.


${DESTDIR}lib/afs/libusd.a: libusd.a
	${INSTALL}   $? $@


install: ${DESTDIR}lib/afs/libusd.a 
	${INSTALL} usd.h ${DESTDIR}include/afs
