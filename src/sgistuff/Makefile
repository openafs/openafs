SHELL=/bin/sh
#
# This must be built after inetd has built ta-rauth.o and after rsh
# XXX want these to end up in /usr/afsws ...
# Note that if afskauthlib.so gets installed on a client
# machine then that will be used ..
#
include ../config/Makefile.${SYS_NAME}
OPTIMIZE=-g
CFLAGS = ${OPTIMIZE} -I${SRCDIR}include ${XCFLAGS}
LDFLAGS = ${OPTIMIZE} ${XLDFLAGS}
LIBDIR = ${DESTDIR}lib/
INSTALL=${SRCDIR}bin/install
AFSLIBS = ${LIBDIR}afs/libkauth.a ${LIBDIR}afs/libprot.a ${LIBDIR}libubik.a \
	  ${LIBDIR}afs/libauth.a ${DESTDIR}lib/librxkad.a ${LIBDIR}afs/libsys.a \
	  ${DESTDIR}lib/libdes.a ${LIBDIR}librx.a ${LIBDIR}liblwp.a \
          ${LIBDIR}afs/libcmd.a ${LIBDIR}afs/libcom_err.a ${LIBDIR}afs/util.a
KAFSLIBS = ${LIBDIR}afs/libkauth.krb.a ${LIBDIR}afs/libprot.a ${LIBDIR}libubik.a \
	  ${LIBDIR}afs/libauth.krb.a ${DESTDIR}lib/librxkad.a ${LIBDIR}afs/libsys.a \
	  ${DESTDIR}lib/libdes.a ${LIBDIR}librx.a ${LIBDIR}liblwp.a \
          ${LIBDIR}afs/libcmd.a ${LIBDIR}afs/libcom_err.a ${LIBDIR}afs/util.a
AUTHFILES=../inetd/ta-rauth.o ../rsh/rcmd.o ../rsh/herror.o
AUTHLIBS=afsauthlib.so afskauthlib.so
TARGETS=$(AUTHLIBS) 

default:$(TARGETS)
	${INSTALL} afsauthlib.so ${DESTDIR}root.client/usr/vice/etc
	${INSTALL} afskauthlib.so ${DESTDIR}root.client/usr/vice/etc

all:$(TARGETS)

install install.noversion: all default

clean:
	rm -f $(AUTHLIBS) $(AUTHFILES) 

afsauthlib.so: sgi_auth.o ${AFSLIBS} ${AUTHFILES}
	$(LD) ${LDFLAGS} -shared -all -o afsauthlib.so sgi_auth.o $(AFSLIBS) ${AUTHFILES}

afskauthlib.so: sgi_auth.o ${KAFSLIBS} ${AUTHFILES}
	$(LD) ${LDFLAGS} -shared -all -o afskauthlib.so sgi_auth.o $(KAFSLIBS) ${AUTHFILES}

../rsh/rcmd.o: ../rsh/rcmd.c
	(cd ../rsh ; make rcmd.o )

../rsh/herror.o: ../rsh/herror.c
	(cd ../rsh ; make herror.o )

sgi_auth.o: sgi_auth.c
	${CC} ${CFLAGS} -c sgi_auth.c
