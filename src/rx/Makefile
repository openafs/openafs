# Copyright 2000, International Business Machines Corporation and others.
# All Rights Reserved.
# 
# This software has been released under the terms of the IBM Public
# License.  For details, see the LICENSE file in the top-level source
# directory or online at http://www.openafs.org/dl/license10.html

SHELL = /bin/sh
COMPONENT=rx
include ../config/Makefile.${SYS_NAME}

KERNELDIR = ../libafs/
UKERNELDIR = ../libuafs/
INSTALL=${SRCDIR}bin/install
MKDIR_IF_NEEDED=[ -d $$1 ] || mkdir -p $$1

#CC=pcc
CFLAGS = ${OPTMZ} -I${SRCDIR}include -DRXDEBUG ${XCFLAGS}

#
# Generic xdr objects (or, at least, xdr stuff that's not newly defined for rx).
# Really the xdr stuff should be in its own directory.
#
XDROBJS = xdr_arrayn.o xdr_rx.o xdr_afsuuid.o

RXOBJS = rx_clock.o rx_event.o rx_user.o rx_lwp.o rx.o rx_null.o rx_globals.o \
	rx_getaddr.o rx_misc.o rx_packet.o rx_rdwr.o  rx_trace.o rx_conncache.o

MULTIOBJS = rx_multi.o

LIBOBJS = ${RXOBJS} ${MULTIOBJS} ${XDROBJS}

BASICINCLS = ${SRCDIR}include/lwp.h ${SRCDIR}include/lock.h \
	     rx_clock.h rx_queue.h rx_event.h

LIBS = librx.a ${SRCDIR}lib/liblwp.a ${SRCDIR}lib/afs/libcmd.a \
	       ${SRCDIR}lib/afs/util.a 

LINK = ${CC} ${CFLAGS} -o $@ $@.o ${LIBS} ${SRCDIR}lib/afs/libsys.a ${XLIBS}

KSRCS = rx.c rx.h rx_clock.c rx_clock.h rx_event.c rx_event.h \
	rx_globals.c rx_globals.h rx_kernel.h \
	rx_null.c rx_null.h rx_queue.h rx_getaddr.c rx_packet.c rx_packet.h \
	rx_multi.h rx_kcommon.h rx_kcommon.c \
	xdr.c xdr.h xdr_array.c xdr_arrayn.c xdr_rx.c rx_misc.c rx_rdwr.c \
	xdr_afsuuid.c rx_trace.h
UKSRCS = $(KSRCS) rx_conncache.c

include ../config/Makefile.version

test tests: install ${DESTDIR}etc/rxdebug rxdumptrace 
#	DEST/bin/washtool object rx/test; cd test; $(MAKE) all DESTDIR=${DESTDIR} SRCDIR=${SRCDIR}

RX_component_version_number.c: AFS_component_version_number.c
	sed 's/cml_version/rx_cml_version/' <AFS_component_version_number.c >RX_component_version_number.c

RX_component_version_number.o: RX_component_version_number.c

xdrs: ${XDROBJS}

${RXOBJS}: ${BASICINCLS} rx.h rx_user.h rx_globals.h

${MULTIOBJS}: rx.h rx_multi.h

${XDROBJS}: xdr.h

rx_user.o: rx.h rx_user.h

rxdebug.o: rxdebug.c

rx_packet.o: rx_packet.c rx_packet.h

rx_rdwr.o: rx_rdwr.c rx.h
#	$(CC) -c -O -I${SRCDIR}include -DRXDEBUG ${XCFLAGS} rx_rdwr.c

rx.o: rx.h rx_user.h

rx_conncache.o: rx.h

rx_trace.o: rx_trace.h 

rxdumptrace: rx_trace.c
	rm -f rxdumptrace
	$(CC) $(CFLAGS) -DDUMPTRACE -o rxdumptrace rx_trace.c  ${LIBS} ${XLIBS}
	rm -f rx_trace.o

rx_getaddr.o: rx.h rx_getaddr.c

rx_globals.o: rx.h rx_user.h rx_globals.h

xdr_rx.o: xdr.h rx.h

xdr_refernce.o: xdr_refernce.c xdr.h

#rx_vab.o: rx_vab.c rx_vab.h rx_user.h ${BASICINCLS}

librx.a: ${LIBOBJS} RX_component_version_number.o
	-rm -f $@
	ar rv $@ RX_component_version_number.o ${LIBOBJS}
	$(RANLIB) $@

${DESTDIR}etc/rxdebug: rxdebug
	${INSTALL} rxdebug ${DESTDIR}etc/rxdebug

rxdebug: rxdebug.o ${DESTDIR}lib/afs/util.a ${DESTDIR}lib/afs/libsys.a
	${LINK}

install.noversion noversion install all system: librx.a includes kinstall ukinstall
	${INSTALL} librx.a ${DESTDIR}lib

includes: rx.h rx_user.h rx_event.h rx_queue.h rx_clock.h xdr.h rx_null.h rx_globals.h
	set ${DESTDIR}include/rx; $(MKDIR_IF_NEEDED)
	${INSTALL} rx_packet.h rx.h rx_user.h rx_event.h rx_queue.h \
	rx_globals.h \
	rx_clock.h rx_multi.h ${DESTDIR}include/rx
	${INSTALL} xdr.h rx_null.h ${DESTDIR}include/rx
	${INSTALL} rx_pthread.h rx_lwp.h rx_misc.h ${DESTDIR}include/rx

kinstall: includes
	set $(KERNELDIR)rx; $(MKDIR_IF_NEEDED)
	$(INSTALL) $(KSRCS) $(KERNELDIR)rx
	@case ${SYS_NAME} in \
	alpha_dux4* ) \
		$(INSTALL) DUX/*.[ch] $(KERNELDIR)rx;; \
	hp_ux* ) \
		$(INSTALL) HPUX/*.[ch] $(KERNELDIR)rx;; \
	*_linux* ) \
		$(INSTALL) LINUX/*.[ch] $(KERNELDIR)rx;; \
	rs_aix* ) \
		$(INSTALL) AIX/*.[ch] $(KERNELDIR)rx;; \
	sgi_* ) \
		$(INSTALL) IRIX/*.[ch] $(KERNELDIR)rx;; \
	sun4x_5* ) \
		$(INSTALL) SOLARIS/*.[ch]  $(KERNELDIR)rx;; \
	* ) \
		echo WARNING: No kernel RX code for sys ${SYS_NAME};; \
	esac

ukinstall webinstall: includes
	set $(UKERNELDIR)rx; $(MKDIR_IF_NEEDED)
	$(INSTALL) $(UKSRCS) $(UKERNELDIR)rx
	$(INSTALL) UKERNEL/*.[ch] $(UKERNELDIR)rx

clean:
	rm -f *.o *.a core *_component_version_number.c
