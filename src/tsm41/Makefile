include ../config/Makefile.${SYS_NAME}
OPTIMIZE=-g
CFLAGS = ${OPTIMIZE} -I${SRCDIR}include ${XCFLAGS}
LIBDIR = ${DESTDIR}lib/
INSTALL=${SRCDIR}bin/install
AFSLIBS = ${LIBDIR}afs/libkauth.a ${LIBDIR}afs/libprot.a ${LIBDIR}libubik.a \
	  ${LIBDIR}afs/libauth.a ${DESTDIR}lib/librxkad.a ${LIBDIR}afs/libsys.a \
	  ${DESTDIR}lib/libdes.a ${LIBDIR}librx.a ${LIBDIR}liblwp.a \
          ${LIBDIR}afs/libcmd.a ${LIBDIR}afs/libcom_err.a ${LIBDIR}afs/util.a ${LIBDIR}afs/libaudit.a
KAFSLIBS = ${LIBDIR}afs/libkauth.krb.a ${LIBDIR}afs/libprot.a ${LIBDIR}libubik.a \
	  ${LIBDIR}afs/libauth.krb.a ${DESTDIR}lib/librxkad.a ${LIBDIR}afs/libsys.a \
	  ${DESTDIR}lib/libdes.a ${LIBDIR}librx.a ${LIBDIR}liblwp.a \
          ${LIBDIR}afs/libcmd.a ${LIBDIR}afs/libcom_err.a ${LIBDIR}afs/util.a ${LIBDIR}afs/libaudit.a
AUTHLIBS=afs_dynamic_auth afs_dynamic_kerbauth
IMPORTS = -bI:/lib/aio.exp -bI:/lib/netinet.exp -bI:/lib/sockets.exp -bI:/lib/statcmd.exp 
LDFLAGS = -eafs_initialize ${IMPORTS}  -lsys -lcsys -lc

install install.noversion: $(AUTHLIBS) 
	${INSTALL} afs_dynamic_auth ${DESTDIR}root.client/usr/vice/etc
	${INSTALL} afs_dynamic_kerbauth ${DESTDIR}root.client/usr/vice/etc
clean:
	rm -f $(AUTHLIBS) $(AUTHFILES) so_locations

afs_dynamic_auth: aix41_auth.o ${AFSLIBS} ${AUTHFILES} 
	$(LD) -o afs_dynamic_auth aix41_auth.o $(AFSLIBS) ${AUTHFILES} ${XLIBS} ${LDFLAGS}

afs_dynamic_kerbauth: aix41_auth_krb.o ${KAFSLIBS} ${AUTHFILES}
	$(LD) -o afs_dynamic_kerbauth aix41_auth_krb.o $(KAFSLIBS) ${AUTHFILES} ${XLIBS} ${LDFLAGS}

aix41_auth.o: aix41_auth.c
	${CC} ${CFLAGS} -c aix41_auth.c

aix41_auth_krb.o: aix41_auth.c
	${CC} ${CFLAGS} -DAFS_KERBEROS_ENV -c aix41_auth.c -o aix41_auth_krb.o
