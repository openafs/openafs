# Copyright 2000, International Business Machines Corporation and others.
# All Rights Reserved.
# 
# This software has been released under the terms of the IBM Public
# License.  For details, see the LICENSE file in the top-level source
# directory or online at http://www.openafs.org/dl/license10.html

# System calls.

SHELL = /bin/sh
COMPONENT=sys
include ../config/Makefile.${SYS_NAME}

MKDIR_IF_NEEDED=[ -d $$1 ] || mkdir -p $$1

UKERNELDIR=../libuafs/
CFLAGS= ${DBUG}  -I${SRCDIR}include  ${XCFLAGS} ${DBG_DEFS}
SFLAGS=-P -I${SRCDIR}include  
LIBS= libsys.a ${SRCDIR}lib/librx.a libsys.a ${SRCDIR}lib/liblwp.a ${SRCDIR}lib/afs/util.a ${XLIBS}
UKSRCS=afsl.exp

OBJECTS= afssyscalls.o syscall.o
RMTOBJS=rmtsysnet.o rmtsysc.o rmtsys.cs.o rmtsys.xdr.o rmtsys.ss.o rmtsyss.o 

include ../config/Makefile.version

all: libsys.a ${DESTDIR}etc/rmtsysd  ${DESTDIR}include/afs/afssyscalls.h \
	${DESTDIR}bin/pagsh  ${DESTDIR}bin/pagsh.krb

${DESTDIR}etc/rmtsysd: rmtsysd
	${INSTALL} -s rmtsysd ${DESTDIR}etc/rmtsysd

${DESTDIR}bin/pagsh: pagsh
	${INSTALL} -s pagsh ${DESTDIR}bin/pagsh

${DESTDIR}bin/pagsh.krb: pagsh.krb
	${INSTALL} -s pagsh.krb ${DESTDIR}bin/pagsh.krb

${DESTDIR}include/afs/afssyscalls.h: afssyscalls.h
	${INSTALL} afssyscalls.h ${DESTDIR}include/afs/afssyscalls.h

libsys.a: ${OBJECTS} ${RMTOBJS} AFS_component_version_number.o
	-rm -f libsys.a
	${AR} r libsys.a ${OBJECTS} ${RMTOBJS} AFS_component_version_number.o
	$(RANLIB) libsys.a
	case "${SYS_NAME}" in				\
	    rs_aix*)		\
		${AR} r libsys.a afsl.exp;;		\
	esac

system: install

kinstall:
	case "${SYS_NAME}" in				\
	    rs_aix*)			\
		${INSTALL} afs.exp ${DESTDIR}lib/afs;;	\
	    sgi_6? ) \
		${INSTALL} xfsattrs.h ../libafs/afs;; \
	    *)						\
		echo No $@ source here;;		\
	esac

install: all kinstall ukinstall
	${INSTALL} libsys.a ${DESTDIR}lib/afs
	case "${SYS_NAME}" in \
	sgi_6? ) \
		${INSTALL} xfsattrs.h ${DESTDIR}include/afs;; \
	esac

ukinstall webinstall: ${UKSRCS}
	set ${UKERNELDIR}afs; $(MKDIR_IF_NEEDED)
	${INSTALL} ${UKSRCS} ${UKERNELDIR}afs

tests:	pagsh pagsh.krb fixit iinc idec icreate iopen istat rmtsysd

syscall.o: syscall.s
	case "$(SYS_NAME)" in \
	 sun4c_51 | sun4c_52 | sun4m_51 | sun4m_52 | sun4c_53 | sun4m_53  | sun4_53 | sun4_52 | sun4_54 | sun4c_54 | sun4m_54 | sun4x_5? | sunx86_54) \
		/usr/ccs/lib/cpp  ${SFLAGS} syscall.s syscall.ss; \
		as -o syscall.o syscall.ss;		\
		rm syscall.ss;;				\
	 sgi_* |ppc_darwin* ) \
                ${CC} ${CFLAGS} -c syscall.s;;          \
	 alpha_osf1 | alpha_osf20 |  alpha_osf30 | alpha_osf32 | alpha_osf32c | alpha_dux?? ) \
		${AS} -P ${CFLAGS} -D_NO_PROTO -DMACH -DOSF -nostdinc -traditional -DASSEMBLER syscall.s; \
		${AS} -o syscall.o syscall.i; \
		rm -f syscall.ss syscall.i;; \
	 hp_ux11? ) \
		touch syscall.o;; \
	i386_fbsd* ) \
		touch syscall.o;; \
	 *) \
		/lib/cpp  ${SFLAGS} syscall.s syscall.ss; \
		as -o syscall.o syscall.ss;		\
		rm syscall.ss;;				\
	esac


afssyscalls.o: afssyscalls.c afssyscalls.h
	${CC} ${CFLAGS} -c afssyscalls.c

rmtsysnet.o rmtsysc.o rmtsyss.o rmtsysd.o: rmtsys.h
rmtsysd: AFS_component_version_number.o

rmtsys.cs.c rmtsys.ss.c rmtsys.xdr.c rmtsys.h: rmtsys.xg
	${SRCDIR}bin/rxgen rmtsys.xg

rmtsysd: rmtsysd.o libsys.a
	${CC} ${CFLAGS} -o rmtsysd rmtsysd.o ${LIBS}


pagsh:	libsys.a AFS_component_version_number.o
	${CC} ${CFLAGS} -c pagsh.c
	${CC} ${CFLAGS} -o pagsh pagsh.o ${LIBS}

pagsh.krb: libsys.a
	${CC} ${CFLAGS} -c pagsh.c -DAFS_KERBEROS_ENV 
	${CC} ${CFLAGS} -o pagsh.krb pagsh.o ${LIBS}

# Test programs.

iinc:	iinc.c 
	case "${SYS_NAME}" in				\
	    sgi_6*) \
		$(CC) -o iinc ${CFLAGS} iinc.c libsys.a;; \
	    *)						\
		$(CC) -o iinc -I${SRCDIR}include iinc.c ${LIBS} ${XLIBS};; \
	esac
idec:	idec.c  AFS_component_version_number.c
	case "${SYS_NAME}" in				\
	    sgi_6* ) \
		$(CC) -o idec ${CFLAGS} idec.c libsys.a;; \
	    *)						\
		$(CC) -o idec -I${SRCDIR}include idec.c ${LIBS} ${XLIBS};; \
	esac
icreate:icreate.c  AFS_component_version_number.c
	case "${SYS_NAME}" in				\
	    sgi_6* ) \
		$(CC) -o icreate ${CFLAGS} icreate.c libsys.a;; \
	    *)						\
		$(CC) -o icreate -I${SRCDIR}include icreate.c ${XLIBS};; \
	esac
iopen:	iopen.c  AFS_component_version_number.c
	case "${SYS_NAME}" in				\
	    sgi_6* ) \
		$(CC) -o iopen ${CFLAGS} iopen.c libsys.a;; \
	    *)						\
		$(CC) -o iopen -I${SRCDIR}include iopen.c ${XLIBS};; \
	esac
iread:	iread.c  AFS_component_version_number.c
	case "${SYS_NAME}" in				\
	    sgi_6* ) \
		${CC} -o iread ${CFLAGS} iread.c libsys.a;; \
	    *)						\
		${CC} -o iread -I${SRCDIR}include iread.c ${XLIBS};; \
	esac
iwrite:	iwrite.c  AFS_component_version_number.c
	case "${SYS_NAME}" in				\
	    sgi_6* ) \
		${CC} -o iwrite ${CFLAGS} iwrite.c libsys.a;; \
	    *)						\
		${CC} -o iwrite -I${SRCDIR}include iwrite.c ${XLIBS};; \
	esac
istat:	istat.c  AFS_component_version_number.c
	case "${SYS_NAME}" in				\
		sgi_6* ) \
		$(CC) -o istat ${CFLAGS} istat.c libsys.a;; \
	    *)						\
		$(CC) -o istat -I${SRCDIR}include istat.c ${XLIBS};; \
	esac
fixit:	fixit.c AFS_component_version_number.c
	$(CC) -o fixit -IDEST/include fixit.c libsys.a 


xfsinode: xfsinode.c  AFS_component_version_number.c
	case "${SYS_NAME}" in \
		sgi_62 | sgi_64 ) \
		$(CC) -o xfsinode ${CFLAGS} xfsinode.c libsys.a;; \
	esac



clean:
	rm -f *.o libsys.a xfsinode iinc idec icreate iopen istat core \
	rmtsysc rmtsyss *.o rmtsys.ss.c rmtsys.cs.c rmtsys.xdr.c rmtsys.h \
	rmtsysd AFS_component_version_number.c

beancount:
	wc -l *.c *.s
