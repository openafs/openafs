#/* Copyright (C) 1998  Transarc Corporation - All rights reserved */
#

include ../config/Makefile.${SYS_NAME}
include ../config/Makefile.version

INSTALL=${SRCDIR}bin/install
CFLAGS = $(DBUG) -I${SRCDIR}include ${XCFLAGS}

########################################################################
# build libprocmgmt.a
LIBOBJS=procmgmt_unix.o

libprocmgmt.a: ${LIBOBJS} AFS_component_version_number.o
	$(RM) -f libprocmgmt.a
	$(AR) r libprocmgmt.a ${LIBOBJS} AFS_component_version_number.o
	$(RANLIB) libprocmgmt.a

procmgmt_unix.o: procmgmt_unix.c procmgmt.h

#######################################################################
system: install

kinstall ukinstall webinstall: install
	@echo No relevant source here.

${DESTDIR}lib/afs/libprocmgmt.a: libprocmgmt.a
	${INSTALL}   $? $@


install: ${DESTDIR}lib/afs/libprocmgmt.a 
	${INSTALL} procmgmt.h ${DESTDIR}include/afs

clean:
	$(RM) -f *.o libprocmgmt.a   \
	 core  AFS_component_version_number.c
