# Copyright 2000, International Business Machines Corporation and others.
# All Rights Reserved.
# 
# This software has been released under the terms of the IBM Public
# License.  For details, see the LICENSE file in the top-level source
# directory or online at http://www.openafs.org/dl/license10.html

# # Auditing is currently only supported for AIX32 machines.  It is
# # integrated with the AIX auditing facility.  The objects, events, 
# # and config files are AIX specific.  The Audit file is not but is
# # included in the rs_aix32 case since other platforms are not
# # supported (if not supported, don't show it).
# ######################################################################

SHELL = /bin/sh

COMPONENT=audit
include ../config/Makefile.${SYS_NAME}
MKDIR_IF_NEEDED=[ -d $$1 ] || mkdir -p $$1

UKERNELDIR = ../libuafs/
LOCALDIR = ${DESTDIR}root.server/usr/afs/local/

audobjs = audit.o

CFLAGS = ${OPTMZ} -I${TOP_SRCDIR}/config -I$(SRCDIR)include ${XCFLAGS}

include ../config/Makefile.version

noversion system: install

UKSRCS = audit.h

all:  libaudit.a 

headers: audit.h
	${INSTALL} audit.h ${DESTDIR}include/afs

install: ukinstall libaudit.a 
	${INSTALL} libaudit.a ${DESTDIR}lib/afs
	${INSTALL} audit.h ${DESTDIR}include/afs
	case ${SYS_NAME} in \
	rs_aix*) \
	   ${INSTALL} Audit              ${LOCALDIR}                     ;  \
	   ${INSTALL} objects.aix.sample ${LOCALDIR}audit/objects.sample ;  \
	   ${INSTALL} events.aix.sample  ${LOCALDIR}audit/events.sample  ;  \
	   ${INSTALL} config.aix.sample  ${LOCALDIR}audit/config.sample  ;; \
        *) ;; \
        esac ;


kinstall:
	@echo No $@ source here.

ukinstall webinstall:
	set ${UKERNELDIR}afs; $(MKDIR_IF_NEEDED)
	${INSTALL} ${UKSRCS} ${UKERNELDIR}afs

doc:
	echo no documents in this directory

libaudit.a: ${audobjs} AFS_component_version_number.o
	rm -f libaudit.a
	ar r libaudit.a ${audobjs} AFS_component_version_number.o
	$(RANLIB) libaudit.a

audit.o: audit.c audit.h 
	${CC} ${CFLAGS} -c audit.c

clean:
	rm -f *.o core AFS_component_version_number.c *.a

beancount:
	wc -l *.c *.h

