#/* Copyright (C) 1998 Transarc Corporation - All rights reserved */


# This is a pthread safe library containing rx, rxkad and des.

SHELL = /bin/sh
COMPONENT=libafsrpc
include ../config/Makefile.${SYS_NAME}

INSTALL = ${SRCDIR}bin/install
CC=${MT_CC}
INCLUDES = -I${SRCDIR}include -I.. -I../rx
CFLAGS = ${OPTMZ} ${DBG} ${INCLUDES} -DRXDEBUG ${MT_CFLAGS}
SFLAGS=-P -I${SRCDIR}include
RX = ../rx
CCRULE = ${CC} ${CFLAGS} -c $?
RXKAD = ../rxkad
RXSTAT = ../rxstat
FSINT = ../fsint
DES = ../des
COMERR = ../comerr
UTIL = ../util
SYS = ../sys

SYSOBJS =\
	syscall.o

UTILOBJS =\
	casestrcpy.o \
	base64.o

COMERROBJS =\
	error_msg.o \
	et_name.o \
	com_err.o

XDROBJS =\
	xdr_arrayn.o \
	xdr_rx.o \
	xdr_afsuuid.o \
	AFS_component_version_number.o

RXOBJS =\
	rx_event.o \
	rx_user.o \
	rx_pthread.o \
	rx.o \
	rx_null.o \
	rx_conncache.o \
	rx_globals.o \
	rx_getaddr.o \
	rx_misc.o \
	rx_packet.o \
	rx_rdwr.o \
	rx_trace.o \
	rx_multi.o

RXSTATOBJS =\
	rxstat.o \
	rxstat.ss.o \
	rxstat.xdr.o \
	rxstat.cs.o

FSINTOBJS =\
	afsint.cs.o \
	afsint.xdr.o \
	afscbint.cs.o \
	afscbint.xdr.o \
	afsaux.o

RXKADOBJS =\
	rxkad_client.o \
	rxkad_server.o \
	rxkad_common.o \
	ticket.o \
	fcrypt.o \
	crypt_conn.o

DESOBJS =\
	des.o \
	cbc_encrypt.o \
	pcbc_encrypt.o \
	cksum.o \
	new_rnd_key.o \
	key_sched.o \
	debug_decl.o \
	quad_cksum.o \
	key_parity.o \
	weak_key.o \
	strng_to_key.o \
	misco.o \
	util.o

LIBOBJS = \
	${RXOBJS} \
	${DESOBJS} \
	${XDROBJS} \
	${RXKADOBJS} \
	${COMERROBJS} \
	${UTILOBJS} \
	${SYSOBJS} \
	${CRYPT_OBJ} \
	${RXSTATOBJS} \
	${FSINTOBJS}

system install:
	if [ -f "../des/Makefile" ] ; then \
		make install.lib ; \
	else \
		make libstub ; \
	fi

install.lib: ${DESTDIR}lib/libafsrpc.a


${DESTDIR}lib/libafsrpc.a: libafsrpc.a
	${INSTALL} libafsrpc.a ${DESTDIR}lib/libafsrpc.a

libafsrpc.a: ${LIBOBJS}
	rm -f libafsrpc.a
	${AR} -r libafsrpc.a ${LIBOBJS}
	${RANLIB} libafsrpc.a
	case "${SYS_NAME}" in					\
	    rs_aix*)						\
		${AR} r libafsrpc.a ../sys/afsl.exp;;		\
        esac

rx_event.o: ${RX}/rx_event.c
	${CCRULE};

rx_user.o: ${RX}/rx_user.c
	${CCRULE};

rx_pthread.o: ${RX}/rx_pthread.c
	${CCRULE};

rx.o: ${RX}/rx.c
	${CCRULE};

rx_conncache.o: ${RX}/rx_conncache.c
	${CCRULE};

rx_null.o: ${RX}/rx_null.c
	${CCRULE};

rx_globals.o: ${RX}/rx_globals.c
	${CCRULE};

rx_getaddr.o: ${RX}/rx_getaddr.c
	${CCRULE};

rx_misc.o: ${RX}/rx_misc.c
	${CCRULE};

rx_packet.o: ${RX}/rx_packet.c
	${CCRULE};

rx_rdwr.o: ${RX}/rx_rdwr.c
	${CCRULE};

rx_trace.o: ${RX}/rx_trace.c
	${CCRULE};

rx_multi.o: ${RX}/rx_multi.c
	${CCRULE};

rxkad_client.o: ${RXKAD}/rxkad_client.c
	${CCRULE};

rxkad_server.o: ${RXKAD}/rxkad_server.c
	${CCRULE};

rxkad_common.o: ${RXKAD}/rxkad_common.c
	${CCRULE};

ticket.o: ${RXKAD}/ticket.c
	${CCRULE};

fcrypt.o: ${RXKAD}/fcrypt.c
	${CCRULE};

crypt_conn.o: ${RXKAD}/crypt_conn.c
	${CCRULE};

AFS_component_version_number.o: ${RX}/AFS_component_version_number.c
	${CCRULE};

xdr.o: ${RX}/xdr.c
	${CCRULE};

xdr_array.o: ${RX}/xdr_array.c
	${CCRULE};

xdr_arrayn.o: ${RX}/xdr_arrayn.c
	${CCRULE};

xdr_float.o: ${RX}/xdr_float.c
	${CCRULE};

xdr_mem.o: ${RX}/xdr_mem.c
	${CCRULE};

xdr_rec.o: ${RX}/xdr_rec.c
	${CCRULE};

xdr_refernce.o: ${RX}/xdr_refernce.c
	${CCRULE};

xdr_rx.o: ${RX}/xdr_rx.c
	${CCRULE};

xdr_update.o: ${RX}/xdr_update.c
	${CCRULE};

xdr_afsuuid.o: ${RX}/xdr_afsuuid.c
	${CCRULE};

# Note that the special case statement for compiling des.c is present
# simply to work around a compiler bug on HP-UX 11.0.  The symptom of
# the problem is that linking the pthread fileserver fails with messages
# such as
#
#   pxdb internal warning: cu[84]: SLT_SRCFILE[411] out of synch
#   Please contact your HP Support representative
#   pxdb internal warning: cu[84]: SLT_SRCFILE[442] out of synch
#   pxdb internal warning: cu[84]: SLT_SRCFILE[450] out of synch
#   pxdb internal warning: cu[84]: SLT_SRCFILE[529] out of synch
#   pxdb internal warning: cu[84]: SLT_SRCFILE[544] out of synch
#   ...
#   pxdb32: internal error. File won't be debuggable (still a valid executable)
#   *** Error exit code 10
#
# The problematic version of pxdb is:
#
#   $ what /opt/langtools/bin/pxdb32
#   /opt/langtools/bin/pxdb32:
#           HP92453-02 A.10.0A HP-UX SYMBOLIC DEBUGGER (PXDB) $Revision: 1.1 $
#
# The problem occurs when -g and -O are both used when compiling des.c.
# The simplest way to work around the problem is to leave out either -g or -O.
# Since des.c is relatively stable I've chosen to eliminate -g rather
# than take any hit in performance.

des.o: ${DES}/des.c
	set -x; \
	case ${SYS_NAME} in \
	hp_ux11*) \
		set X `echo ${CCRULE} | sed s/-g//`; shift; \
		"$$@" \
		;; \
	*) \
		${CCRULE} \
		;; \
	esac

crypt.o: ${DES}/crypt.c
	${CCRULE};

cbc_encrypt.o: ${DES}/cbc_encrypt.c
	${CCRULE};

pcbc_encrypt.o: ${DES}/pcbc_encrypt.c
	${CCRULE};

cksum.o: ${DES}/cksum.c
	${CCRULE};

new_rnd_key.o: ${DES}/new_rnd_key.c
	${CCRULE};

key_sched.o: ${DES}/key_sched.c
	${CCRULE};

debug_decl.o: ${DES}/debug_decl.c
	${CCRULE};

quad_cksum.o: ${DES}/quad_cksum.c
	${CCRULE};

key_parity.o: ${DES}/key_parity.c
	${CCRULE};

weak_key.o: ${DES}/weak_key.c
	${CCRULE};

strng_to_key.o: ${DES}/strng_to_key.c
	${CCRULE};

misco.o: ${DES}/misco.c
	${CCRULE} -DDONT_INCL_MAIN;

util.o: ${DES}/util.c
	${CCRULE};

error_msg.o: ${COMERR}/error_msg.c
	${CCRULE};

et_name.o: ${COMERR}/et_name.c
	${CCRULE};

com_err.o: ${COMERR}/com_err.c
	${CCRULE};

casestrcpy.o: ${UTIL}/casestrcpy.c
	${CCRULE};

base64.o: ${UTIL}/base64.c
	${CCRULE};

syscall.o: ${SYS}/syscall.s
	case "$(SYS_NAME)" in \
	     sun4x_5*) \
		/usr/ccs/lib/cpp  ${SFLAGS} ${SYS}/syscall.s syscall.ss; \
		as -o syscall.o syscall.ss;		\
		rm syscall.ss;;				\
	 sgi_* ) \
                ${CC} ${CFLAGS} -c ${SYS}/syscall.s;;          \
	 alpha_dux?? ) \
		${AS} -P ${CFLAGS} -D_NO_PROTO -DMACH -DOSF -nostdinc -traditional -DASSEMBLER ${SYS}/syscall.s; \
		${AS} -o syscall.o syscall.i; \
		rm -f syscall.ss syscall.i;; \
	 *) \
		/lib/cpp  ${SFLAGS} ${SYS}/syscall.s syscall.ss; \
		as -o syscall.o syscall.ss;		\
		rm syscall.ss;;				\
	esac

rxstat.o: ${RXSTAT}/rxstat.c
	${CCRULE};

rxstat.cs.o: ${RXSTAT}/rxstat.cs.c
	${CCRULE};

rxstat.ss.o: ${RXSTAT}/rxstat.ss.c
	${CCRULE};

rxstat.xdr.o: ${RXSTAT}/rxstat.xdr.c
	${CCRULE};

afsint.cs.o: ${FSINT}/afsint.cs.c
	${CCRULE};

afsint.xdr.o: ${FSINT}/afsint.xdr.c
	${CCRULE};

afscbint.cs.o: ${FSINT}/afscbint.cs.c
	${CCRULE};

afscbint.xdr.o: ${FSINT}/afscbint.xdr.c
	${CCRULE};

afsaux.o: ${FSINT}/afsaux.c
	${CCRULE};

clean:
	rm -f *.o libafsrpc*



RM     = /bin/rm
DESPAR =../../../DESLIB/dest/
libstub:
	echo "Installing libafsrpc stubs from ${DESPAR}:" `(csh -c 'cd ${DESPAR};pwd')`
	${RM} -f ${DESTDIR}lib/libafsrpc.a
	${INSTALL} ${DESPAR}lib/libafsrpc.a ${DESTDIR}lib/libafsrpc.a
