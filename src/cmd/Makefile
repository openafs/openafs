# Copyright 2000, International Business Machines Corporation and others.
# All Rights Reserved.
# 
# This software has been released under the terms of the IBM Public
# License.  For details, see the LICENSE file in the top-level source
# directory or online at http://www.openafs.org/dl/license10.html

# */

SHELL = /bin/sh
COMPONENT=cmd
include ../config/Makefile.${SYS_NAME}

COMPILE_ET = ${SRCDIR}bin/compile_et

INCLS=cmd.h ${XINCLS}
LIBOBJS=cmd_errors.o cmd.o 
LIB64OBJS=cmd_errors64.o cmd64.o 

CFLAGS= ${OPTMZ} -I${SRCDIR}include ${XCFLAGS}

include ../config/Makefile.version

system install noversion: install.noversion

all: libcmd.a

test tests: 
	(cd test; $(MAKE) )

cmd.h cmd_errors.c: cmd_errors.et cmd.p.h
	rm -f cmd.h cmd_errors.c; ${COMPILE_ET} cmd_errors -h cmd

cmd_errors.o: cmd_errors.c

cmd.o: cmd.c ${INCLS}

AFS_component_version_number64.o: AFS_component_version_number.c
	${CC} ${OPTMZ} -I${SRCDIR}include ${XCFLAGS64} -c -o AFS_component_version_number64.o AFS_component_version_number.c

cmd_errors64.o: cmd_errors.c
	${CC} ${OPTMZ} -I${SRCDIR}include ${XCFLAGS64} -c -o cmd_errors64.o cmd_errors.c

cmd64.o: cmd.c ${INCLS}
	${CC} ${OPTMZ} -I${SRCDIR}include ${XCFLAGS64} -c -o cmd64.o cmd.c


libcmd64.a: ${LIB64OBJS} AFS_component_version_number64.o
	-rm -f libcmd64.a
	ar r libcmd64.a ${LIB64OBJS} AFS_component_version_number64.o
	$(RANLIB) libcmd64.a

libcmd.a: ${LIBOBJS} AFS_component_version_number.o
	-rm -f libcmd.a
	ar r libcmd.a ${LIBOBJS} AFS_component_version_number.o
	$(RANLIB) libcmd.a

kinstall:
	@echo No kernel source here.

ukinstall webinstall: install

install64: libcmd64.a
	${INSTALL} libcmd64.a  ${DESTDIR}lib/afs/libcmd64.a
	${INSTALL} cmd.h ${DESTDIR}include/afs/cmd.h

install.noversion: all
	${INSTALL} libcmd.a ${DESTDIR}lib/afs/libcmd.a
	${INSTALL} cmd.h ${DESTDIR}include/afs/cmd.h

clean:
	rm -f  *.a *.o ctest dtest cmd.h cmd_errors.c core AFS_component_version_number.c

