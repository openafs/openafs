# Copyright 2000, International Business Machines Corporation and others.
# All Rights Reserved.
# 
# This software has been released under the terms of the IBM Public
# License.  For details, see the LICENSE file in the top-level source
# directory or online at http://www.openafs.org/dl/license10.html

# */


SHELL = /bin/sh
COMPONENT=rxkad
include ../config/Makefile.${SYS_NAME}

KERNELDIR = ../libafs/
UKERNELDIR = ../libuafs/
COMPILE_ET = ${SRCDIR}bin/compile_et
MKDIR_IF_NEEDED=[ -d $$1 ] || mkdir -p $$1

CFLAGS = ${OPTMZ} -I${SRCDIR}include ${XCFLAGS}
PE = ../permit_xprt.h
INCLS= ${SRCDIR}include/rx/rx.h \
       ${SRCDIR}include/rx/xdr.h \
       rxkad.h ${PE}

OBJS= rxkad_client.o rxkad_server.o rxkad_common.o ticket.o rxkad_errs.o

REGOBJS=fcrypt.o crypt_conn.o

KSRCS=private_data.h fcrypt.h fcrypt.c crypt_conn.c rxkad_client.c rxkad_common.c rxkad.h sboxes.h

UKSRCS=$(KSRCS) rxkad.h ticket.c rxkad_server.c lifetimes.h private_data.h rxkad_errs.c

include ../config/Makefile.version

noversion: install

all: librxkad.a

system: install

clean: 
	rm -f *.o tcrypt core *.a rxkad_errs.c rxkad.h AFS_component_version_number.c

librxkad.a: $(OBJS) ${REGOBJS} AFS_component_version_number.o
	-rm -f librxkad.a
	$(AR) rv librxkad.a $(OBJS) ${REGOBJS} AFS_component_version_number.o
	$(RANLIB) librxkad.a

crypt_conn.o: fcrypt.h private_data.h crypt_conn.c ${INCLS}

rxkad_client.o: fcrypt.h private_data.h rxkad_client.c ${INCLS}

rxkad_server.o: fcrypt.h private_data.h rxkad_server.c ${INCLS}

rxkad_common.o: fcrypt.h private_data.h rxkad_common.c ${INCLS}

rxkad_errs.o: rxkad_errs.c

rxkad_errs.c rxkad.h: rxkad_errs.et rxkad.p.h
	rm -f rxkad.h rxkad_errs.c; ${COMPILE_ET} rxkad_errs -h rxkad

ticket.o: ticket.c lifetimes.h ${INCLS}

fcrypt.o: fcrypt.c fcrypt.h sboxes.h rxkad.h ${PE}
	${CC} ${CFLAGS} -c fcrypt.c

kinstall: ${KSRCS}
	${INSTALL} $(KSRCS) ${KERNELDIR}afs

ukinstall webinstall: ${UKSRCS}
	set ${UKERNELDIR}afs; $(MKDIR_IF_NEEDED)
	set ${UKERNELDIR}rx; $(MKDIR_IF_NEEDED)
	${INSTALL} $(UKSRCS) ${UKERNELDIR}afs
	${INSTALL} rxkad.h ${UKERNELDIR}rx

install: all kinstall ukinstall
	rm -f ${DESTDIR}lib/librxkad.a
	${INSTALL} librxkad.a ${DESTDIR}lib
	${INSTALL} rxkad.h ${DESTDIR}include/rx

tcrypt: tcrypt.o librxkad.a 
	${CC} -o tcrypt tcrypt.o librxkad.a
tcrypt.o: tcrypt.c AFS_component_version_number.o

test:
	cd test; $(MAKE)

# These sources are kept in a separate directory so that we can use an
# ACL to comply with source export restrictions.

crypt_conn.c:	domestic/crypt_conn.c
	rm -f $@
	cp domestic/crypt_conn.c $@

fcrypt.c:	domestic/fcrypt.c
	rm -f $@
	cp domestic/fcrypt.c $@

fcrypt.h:	domestic/fcrypt.h
	rm -f $@
	cp domestic/fcrypt.h $@

sboxes.h:	domestic/sboxes.h
	rm -f $@
	cp domestic/sboxes.h $@

tcrypt.c:	domestic/tcrypt.c
	rm -f $@
	cp domestic/tcrypt.c $@
