SHELL=/bin/sh
COMPONENT=inetd
include ../config/Makefile.${SYS_NAME}

INCLUDES = -I${TOP_SRCDIR}/config -I${DESTDIR}include
DEFINES = ${INCLUDES}
LIBDIR = ${DESTDIR}lib/
AFSLIBS =  ${LIBDIR}afs/libkauth.a ${LIBDIR}libubik.a\
		   ${LIBDIR}afs/libauth.a ${LIBDIR}afs/libsys.a \
		   ${LIBDIR}librx.a \
		   ${LIBDIR}afs/libsys.a \
		   ${LIBDIR}liblwp.a ${DESTDIR}lib/librxkad.a\
		   ${DESTDIR}lib/libdes.a ${LIBDIR}afs/libcmd.a \
		   ${LIBDIR}afs/libcom_err.a ${LIBDIR}afs/util.a 
LIBRES = 
LIBS = ${AFSLIBS} ${LIBRES} ${XLIBS}
OBJS = inetd.o setenv.o getenv.o
CFLAGS = ${DEFINES} ${DBUG} ${XCFLAGS}

include ../config/Makefile.version

noversion: install

# ta-rauth.o is used by rlogin, rsh, rcp, rlogind
all : inetd ta-rauth.o

system: install

inetd: ${OBJS}
	set -x; \
	case "${SYS_NAME}" in \
	alpha_osf1 | alpha_osf20 |  alpha_osf30 | alpha_dux?? ) \
		${CC} -o $@ ${DEFINES} ${OBJS} ${LIBS} -lutil ;; \
	sgi_6* ) \
		${CC} -o $@ ${CFLAGS} ${OBJS} ${LIBS} ;; \
	*) \
		${CC} -o $@ ${DEFINES} ${OBJS} ${LIBS} ;; \
	esac

inetd.o: inetd.c AFS_component_version_number.c
ta-rauth.o: ta-rauth.c

clean:
	rm -f inetd *.o *.BAK *~ core AFS_component_version_number.c

install: all
	${INSTALL} -s -m 755 inetd ${DESTDIR}etc/inetd
	set -x; \
	case "${SYS_NAME}" in \
	sun4* ) \
		${INSTALL} -m 644 -f inetd.conf.solaris ${DESTDIR}etc/inetd.conf ;; \
	sgi_* ) \
		${INSTALL} -m 644 -f inetd.conf.sgi ${DESTDIR}etc/inetd.conf ;; \
	alpha_dux?? ) \
		${INSTALL} -m 644 -f inetd.conf.dux40 ${DESTDIR}etc/inetd.conf ;; \
	* ) \
		${INSTALL} -m 644 inetd.conf ${DESTDIR}etc/inetd.conf ;; \
	esac
