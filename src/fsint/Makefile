# Copyright 2000, International Business Machines Corporation and others.
# All Rights Reserved.
# 
# This software has been released under the terms of the IBM Public
# License.  For details, see the LICENSE file in the top-level source
# directory or online at http://www.openafs.org/dl/license10.html

COMPONENT=fsint
include ../config/Makefile.${SYS_NAME}
SHELL = /bin/sh
KERNELDIR = ../libafs/
UKERNELDIR = ../libuafs/
MKDIR_IF_NEEDED=[ -d $$1 ] || mkdir -p $$1

OBJS=afsaux.o afscbint.cs.o afscbint.ss.o afscbint.xdr.o afsint.cs.o afsint.ss.o afsint.xdr.o
CFLAGS=${DBUG} -I${SRCDIR}include ${XCFLAGS}
KDSRCS=Kcallback.ss.c Kcallback.h Kvice.cs.c Kvice.h Kvice.xdr.c afscbint.h \
afsint.h
KSRCS=${KDSRCS} afsaux.c
UKSRCS=${KSRCS}
UTILS=$(SRCDIR)/bin/rxgen

include ../config/Makefile.version

noversion: install

all: libafsint.a $(KSRCS)

libafsint.a: ${OBJS} AFS_component_version_number.o
	-rm -f $@
	$(AR) rv $@ ${OBJS} AFS_component_version_number.o
	$(RANLIB) $@

afsaux.o: afsaux.c afsint.h

Kcallback.cs.c: common.xg afscbint.xg Kvice.h Kcallback.h $(UTILS)
	${SRCDIR}bin/rxgen -x -k -C -o Kcallback.cs.c afscbint.xg

Kcallback.ss.c: common.xg afscbint.xg Kvice.h Kcallback.h $(UTILS)
	${SRCDIR}bin/rxgen -x -k -S -o Kcallback.ss.c afscbint.xg

Kcallback.xdr.c: common.xg afscbint.xg Kvice.h Kcallback.h $(UTILS)
	${SRCDIR}bin/rxgen -x -k -y -c -o Kcallback.xdr.c afscbint.xg

Kvice.cs.c: common.xg afsint.xg Kvice.h  $(UTILS)
	${SRCDIR}bin/rxgen -x -k -C -o Kvice.cs.c afsint.xg

Kvice.ss.c: common.xg afsint.xg Kvice.h $(UTILS)
	${SRCDIR}bin/rxgen -x -k -S -o Kvice.ss.c afsint.xg

Kvice.xdr.c: common.xg afsint.xg $(UTILS)
	${SRCDIR}bin/rxgen -x -k -c -o Kvice.xdr.c afsint.xg

Kvice.h: common.xg afsint.xg $(UTILS)
	${SRCDIR}bin/rxgen -x -k -h -o Kvice.h afsint.xg

Kcallback.h: common.xg afscbint.xg $(UTILS)
	${SRCDIR}bin/rxgen -x -k -h -o Kcallback.h afscbint.xg

afscbint.cs.c: common.xg afscbint.xg afsint.h afscbint.h $(UTILS)
	${SRCDIR}bin/rxgen -x -C -o afscbint.cs.c afscbint.xg

afscbint.ss.c: common.xg afscbint.xg afsint.h afscbint.h $(UTILS)
	${SRCDIR}bin/rxgen -x -S -o afscbint.ss.c afscbint.xg

afscbint.xdr.c: common.xg afscbint.xg afsint.h afscbint.h $(UTILS)
	${SRCDIR}bin/rxgen -x -y -c -o afscbint.xdr.c afscbint.xg

afsint.cs.c: common.xg afsint.xg afsint.h $(UTILS)
	${SRCDIR}bin/rxgen -x -C -o afsint.cs.c afsint.xg

afsint.ss.c: common.xg afsint.xg afsint.h $(UTILS)
	${SRCDIR}bin/rxgen -x -S -o afsint.ss.c afsint.xg

afsint.xdr.c: common.xg afsint.xg $(UTILS)
	${SRCDIR}bin/rxgen -x -c -o afsint.xdr.c afsint.xg

afsint.h: common.xg afsint.xg $(UTILS)
	${SRCDIR}bin/rxgen -x -h -o afsint.h afsint.xg

afscbint.h: common.xg afscbint.xg $(UTILS)
	${SRCDIR}bin/rxgen -x -h -o afscbint.h afscbint.xg

system: install

kinstall: ${KSRCS}
	set ${KERNELDIR}afsint; $(MKDIR_IF_NEEDED)
	${INSTALL} $(KSRCS) ${KERNELDIR}afsint
	${INSTALL} $(SRCDIR)include/afs/rxgen_consts.h ${KERNELDIR}afsint
	-${INSTALL} ${KERNELDIR}rx/rx.h ${KERNELDIR}afsint/rx.h
	${INSTALL} afscbint.h ${KERNELDIR}afs

ukinstall webinstall: ${UKSRCS}
	set ${UKERNELDIR}afsint; $(MKDIR_IF_NEEDED)
	${INSTALL} $(UKSRCS) ${UKERNELDIR}afsint
	${INSTALL} $(SRCDIR)include/afs/rxgen_consts.h ${UKERNELDIR}afsint
	-${INSTALL} ${UKERNELDIR}rx/rx.h ${UKERNELDIR}afsint/rx.h
	${INSTALL} afscbint.h ${UKERNELDIR}afs

install:  all  kinstall ukinstall
	${INSTALL} libafsint.a ${DESTDIR}lib/afs
	${INSTALL} afsint.h ${DESTDIR}include/afs
	${INSTALL} afscbint.h ${DESTDIR}include/afs

clean:
	rm -f *.o *.cs.c *.ss.c *.er.c afsint.h afsint.xdr.c libafsint.a $(KDSRCS) core AFS_component_version_number.c afscbint.xdr.c
