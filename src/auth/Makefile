
#/* Copyright (C) 1990, 1989 Transarc Corporation - All rights reserved */
#
# (C) COPYRIGHT IBM CORPORATION 1987, 1989
# LICENSED MATERIALS - PROPERTY OF IBM
#

SHELL = /bin/sh
COMPONENT=auth

include ../config/Makefile.${SYS_NAME}

KERNELDIR = ../libafs/
UKERNELDIR = ../libuafs/
INSTALL = ${SRCDIR}bin/install
COMPILE_ET = ${SRCDIR}bin/compile_et
OBJS= cellconfig.o ktc.o userok.o writeconfig.o authcon.o \
    acfg_errors.o ktc_errors.o # comktc.o comvice.o comauth.o 
KOBJS= cellconfig.o ktc.krb.o userok.o writeconfig.o authcon.o \
    acfg_errors.o ktc_errors.o # comktc.o comvice.o comauth.o 

CFLAGS=-g -I${SRCDIR}include ${XCFLAGS} 
LDIRS= 
LIBS= libauth.a ${DESTDIR}lib/afs/libsys.a \
      ${DESTDIR}lib/librxkad.a ${DESTDIR}lib/libdes.a \
      ${DESTDIR}lib/librx.a ${DESTDIR}lib/afs/libsys.a \
      ${DESTDIR}lib/liblwp.a ${SRCDIR}lib/afs/util.a ${XLIBS}
INCLS=cellconfig.h auth.h keys.h # comauth.h
KSRCS=auth.h
UKSRCS=${KSRCS} cellconfig.h acfg_errors.c keys.h cellconfig.c \
       ktc.c authcon.c ktc_errors.c

include ../config/Makefile.version

system install noversion: install.noversion

all: libauth.a libauth.krb.a setkey

cellconfig.o: cellconfig.c ${INCLS}
#comktc.o: comktc.c ${INCLS} ${SRCDIR}include/afs/vice.h
#comvice.o: comvice.c ${INCLS}
writeconfig.o: writeconfig.c ${INCLS}
authcon.o: authcon.c ${INCLS}
userok.o: userok.c ${INCLS}
cellconfig.o: cellconfig.c ${INCLS}
copyauth.o: copyauth.c ${INCLS} AFS_component_version_number.o
setkey.o: setkey.c ${INCLS} AFS_component_version_number.o
#comauth.o: comauth.c ${INCLS} ${SRCDIR}include/afs/vice.h

ktc.krb.o: ktc.c ${INCLS} ${SRCDIR}include/afs/vice.h
	${CC} ${CFLAGS} -DMAYBE_NO_KTC -DAFS_KERBEROS_ENV -c ktc.c
	mv ktc.o ktc.krb.o

ktc.o: ktc.c ${INCLS} ${SRCDIR}include/afs/vice.h
	${CC} ${CFLAGS} -DMAYBE_NO_KTC -c ktc.c
# : ${SRCDIR}include/afs/ktc.h
#	${CC} ${CFLAGS} -c ktc.c

libauth.a: $(OBJS) AFS_component_version_number.o
	-rm -f libauth.a
	ar rv libauth.a $(OBJS) AFS_component_version_number.o
	$(RANLIB) libauth.a

libauth.krb.a: $(KOBJS) AFS_component_version_number.o
	-rm -f libauth.krb.a
	ar rv libauth.krb.a $(KOBJS) AFS_component_version_number.o
	$(RANLIB) libauth.krb.a

copyauth: copyauth.o
	$(CC) $(CFLAGS) $(LDIRS) -o copyauth copyauth.o ${LIBS}

setkey: setkey.o
	${CC} $(CFLAGS) $(LDIRS) -o setkey setkey.o ${LIBS}

acfg_errors.o: acfg_errors.c

acfg_errors.c cellconfig.h: acfg_errors.et cellconfig.p.h
	rm -f cellconfig.h acfg_errors.c; ${COMPILE_ET} acfg_errors -h cellconfig

ktc_errors.o: ktc_errors.c

ktc_errors.c auth.h: ktc_errors.et auth.p.h
	rm -f auth.h ktc_errors.c; ${COMPILE_ET} ktc_errors -h auth

test: 
	cd test; $(MAKE)

kinstall: ${KSRCS}
	${INSTALL} ${KSRCS} ${KERNELDIR}afs

ukinstall webinstall: ${UKSRCS}
	${INSTALL} ${UKSRCS} ${UKERNELDIR}afs

${DESTDIR}etc/copyauth: copyauth
	${INSTALL} copyauth ${DESTDIR}etc/copyauth

install.noversion: all kinstall ukinstall ${DESTDIR}etc/copyauth
	${INSTALL} libauth.a ${DESTDIR}lib/afs/libauth.a
	${INSTALL} libauth.krb.a ${DESTDIR}lib/afs/libauth.krb.a
	${INSTALL} auth.h ${DESTDIR}include/afs
#	${INSTALL} comauth.h ${DESTDIR}include/afs
	${INSTALL} cellconfig.h ${DESTDIR}include/afs
	${INSTALL} keys.h ${DESTDIR}include/afs

clean:
	rm -f *.o *.a copyauth testcellconf setkey auth.h cellconfig.h acfg_errors.c ktc_errors.c core\
	AFS_component_version_number.c
