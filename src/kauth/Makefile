# Copyright 2000, International Business Machines Corporation and others.
# All Rights Reserved.
# 
# This software has been released under the terms of the IBM Public
# License.  For details, see the LICENSE file in the top-level source
# directory or online at http://www.openafs.org/dl/license10.html

# */


SHELL = /bin/sh
COMPONENT=kauth
include ../config/Makefile.${SYS_NAME}

UKERNELDIR = ../libuafs/
INSTALL=${SRCDIR}bin/install
COMPILE_ET = ${SRCDIR}bin/compile_et
MKDIR_IF_NEEDED=[ -d $$1 ] || mkdir -p $$1

CFLAGS = ${OPTMZ} -I${SRCDIR}include ${XCFLAGS}
LDFLAGS = ${OPTMZ} ${XLDFLAGS}
PE = ../permit_xprt.h
INCLS=${SRCDIR}include/ubik.h \
             ${SRCDIR}include/lwp.h \
             ${SRCDIR}include/lock.h  \
             ${SRCDIR}include/rx/rx.h \
             ${SRCDIR}include/rx/xdr.h \
	     ${SRCDIR}include/rx/rxkad.h \
	     ${SRCDIR}include/afs/com_err.h \
	kaserver.h kauth.h kautils.h 
deslib=${SRCDIR}lib/libdes.a
rxkadlib=${SRCDIR}lib/librxkad.a
authlib=${SRCDIR}lib/afs/libauth.a
cmdlib=${SRCDIR}lib/afs/libcmd.a
utilib=${SRCDIR}lib/afs/util.a
auditlib=${SRCDIR}lib/afs/libaudit.a

LIBS =	${SRCDIR}lib/libubik.a \
	${SRCDIR}lib/afs/libprot.a \
	${authlib} \
	${rxkadlib} \
	${SRCDIR}lib/afs/libsys.a \
	${SRCDIR}lib/librxstat.a \
	${SRCDIR}lib/librx.a \
	${SRCDIR}lib/afs/libsys.a  \
	${SRCDIR}lib/liblwp.a \
	${deslib} \
	${cmdlib} \
	${SRCDIR}lib/afs/libcom_err.a \
	${utilib} \
	$(DBM)

UKSRCS=authclient.c user.c kautils.h kaserver.h kaaux.c token.c \
       kalocalcell.c client.c kaerrors.c

KLIBS = ${SRCDIR}lib/libubik.a ${SRCDIR}lib/afs/libprot.a \
       ${SRCDIR}lib/afs/libauth.krb.a ${rxkadlib} ${SRCDIR}lib/afs/libsys.a ${SRCDIR}lib/librx.a \
       ${SRCDIR}lib/afs/libsys.a  \
       ${SRCDIR}lib/liblwp.a ${deslib} ${cmdlib} \
       ${SRCDIR}lib/afs/libcom_err.a ${utilib}

OBJS=kauth.xdr.o kauth.cs.o kaaux.o client.o authclient.o token.o kautils.o kalocalcell.o kaerrors.o user.o krb_tf.o 
KOBJS=kauth.xdr.o kauth.cs.o kaaux.o client.o authclient.o token.o kautils.o kalocalcell.o kaerrors.o user.krb.o krb_tf.o

include ../config/Makefile.version

noversion: install

all: knfs kaserver kas klog klog.krb kpasswd klogin klogin.krb rebuild kdb kpwvalid

test tests: install
	cd test; $(MAKE)

clean:
	rm -f *.o kaserver kas klog multiklog kpasswd klogin rebuild kauth.cs.c kauth.ss.c kauth.xdr.c kauth.h kautils.h kaerrors.c *.a core kpwvalid AFS_component_version_number.c

kaserver: kautils.o kalocalcell.o kadatabase.o kaprocs.o kalog.o kauth.ss.o kauth.xdr.o kaserver.o kaaux.o krb_udp.o kaauxdb.o $(LIBS) ${auditlib}
	${CC} ${LDFLAGS} -o kaserver kaserver.o kautils.o kalocalcell.o kadatabase.o krb_udp.o kaprocs.o kalog.o kauth.ss.o kauth.xdr.o kaaux.o kaauxdb.o $(LIBS) ${XLIBS} ${auditlib}

kaserver.o: kaserver.c ${INCLS} AFS_component_version_number.o

kautils.o: kautils.c ${INCLS}

kalocalcell.o: kalocalcell.c ${INCLS}

kadatabase.o: kadatabase.c ${INCLS}
	${CC} ${CFLAGS} -c kadatabase.c -DSPECIAL=1

xkaprocs: kaprocs.c ${INCLS}
	${CC} ${CFLAGS} -c kaprocs.c -DAUTOCPWINTERVAL=30 -DAUTOCPWUPDATES=3

# Installations using Auth2 conversion server should specify the GETPASSWORD
# flag.  This also means logins to machines running this server must be
# tightly controlled.
#	${CC} ${CFLAGS} -c kaprocs.c -DSPECIAL=1 -DGETPASSWORD

kaprocs.o: kaprocs.c ${INCLS} ${PE} kaport.h
	${CC} ${CFLAGS} -c kaprocs.c -DSPECIAL=1

authclient.o: authclient.c ${INCLS} ${PE}


#authserver.o: authserver.c ${INCLS}

kaerrors.o: kaerrors.c

kaerrors.c kautils.h: kaerrors.et kautils.p.h
	rm -f kautils.h kaerrors.c; ${COMPILE_ET} kaerrors -h kautils

kauth.ss.o: kauth.ss.c
kauth.cs.o: kauth.cs.c
kauth.xdr.o: kauth.xdr.c kauth.h

kauth.ss.c kauth.cs.c kauth.xdr.c kauth.h: kauth.rg
	${SRCDIR}bin/rxgen -x kauth.rg

Kkauth.cs.c: kauth.rg Kkauth.h
	${SRCDIR}bin/rxgen -x -k -C -o Kkauth.cs.c kauth.rg

Kkauth.xdr.c: kauth.rg
	${SRCDIR}bin/rxgen -x -k -c -o Kkauth.xdr.c kauth.rg

Kkauth.h: kauth.rg
	${SRCDIR}bin/rxgen -x -k -h -o Kkauth.h kauth.rg

libkauth.a: $(OBJS) AFS_component_version_number.o
	-rm -f libkauth.a
	ar rv libkauth.a $(OBJS) AFS_component_version_number.o
	$(RANLIB) libkauth.a


libkauth.krb.a: $(KOBJS) AFS_component_version_number.o
	-rm -f libkauth.krb.a
	ar rv libkauth.krb.a $(KOBJS) AFS_component_version_number.o
	$(RANLIB) libkauth.krb.a


read_passwd.o: read_passwd.c
	echo These changes to read_password.c need to be mailed to the Kerberos people; ${CC} ${CFLAGS} -c read_passwd.c

token.o: token.c ${INCLS}

client.o: client.c ${INCLS} AFS_component_version_number.o

kas.o: kas.c ${INCLS} AFS_component_version_number.o

admin_tools.o: admin_tools.c ${INCLS} kaport.h

kkids.o: kkids.c ${INCLS}

krb_tf.o: krb_tf.c ${INCLS}

kas: kauth.h kautils.h admin_tools.o libkauth.a $(LIBS) kas.o kkids.o
	${CC} ${LDFLAGS} -o kas kas.o admin_tools.o kkids.o libkauth.a \
			${LIBS} ${XLIBS}

klog: AFS_component_version_number.o kauth.h kautils.h libkauth.a $(LIBS) \
	klog.o
	${CC} ${LDFLAGS} -o klog klog.o libkauth.a ${LIBS} ${XLIBS}

klog.o: klog.c kauth.h kautils.h AFS_component_version_number.o

klog.krb: kauth.h kautils.h libkauth.krb.a $(KLIBS) klog.o
	${CC} ${LDFLAGS} -o klog.krb klog.o libkauth.krb.a ${KLIBS} ${XLIBS}

knfs: kauth.h kautils.h libkauth.a $(LIBS) knfs.o
	${CC} ${LDFLAGS} -o knfs knfs.o libkauth.a ${LIBS} ${XLIBS}
knfs.o: knfs.c AFS_component_version_number.o

klogin.o: klogin.c ${INCLS} AFS_component_version_number.o
	${CC} ${CFLAGS} -c klogin.c -DKAUTH

klogin: libkauth.a $(LIBS) klogin.o
	${CC} ${LDFLAGS} -o klogin klogin.o libkauth.a ${LIBS} ${XLIBS}

klogin.krb: libkauth.a $(KLIBS) klogin.o
	${CC} ${LDFLAGS} -o klogin.krb klogin.o libkauth.krb.a ${KLIBS} ${XLIBS}

kpasswd.o: kauth.h kautils.h ${INCLS} kpasswd.c AFS_component_version_number.o
	${CC} ${CFLAGS} -c kpasswd.c 

kpasswd: kauth.h kautils.h libkauth.a $(LIBS) kpasswd.o kkids.o
	${CC} ${LDFLAGS} -o kpasswd kpasswd.o kkids.o libkauth.a \
	${LIBS} ${XLIBS}

kpwvalid.o: kpwvalid.c AFS_component_version_number.o
	${CC} ${CFLAGS} -c kpwvalid.c

kpwvalid: kpwvalid.o $(LIBS)
	${CC} ${LDFLAGS} -o kpwvalid kpwvalid.o ${LIBS} ${XLIBS}

user.krb.o: user.c ${INCLS} ${SRCDIR}include/afs/vice.h
	${CC} ${CFLAGS} -DAFS_KERBEROS_ENV -c user.c
	mv user.o user.krb.o

user.o: user.c ${INCLS} ${SRCDIR}include/afs/vice.h
	${CC} ${CFLAGS} -c user.c

kdb: kdb.o ${INCLS}
	${CC} ${LDFLAGS} -o kdb kdb.o libkauth.a ${LIBS} ${XLIBS}
kdb.o: kdb.c AFS_component_version_number.o

krb_udp.o: krb_udp.c kaserver.h kautils.h kauth.h prot.h \
	   ${SRCDIR}include/lwp.h ${PE} AFS_component_version_number.o

krb_udp: krb_udp.o libkauth.a
	${CC} ${LDFLAGS} -o krb_udp krb_udp.o libkauth.a ${SRCDIR}lib/liblwp.a ${deslib} ${rxkadlib} ${SRCDIR}lib/afs/libcom_err.a ${utilib} ${auditlib}

rebuild.o: rebuild.c $(INCLS) AFS_component_version_number.o

rebuild: rebuild.o kautils.o ${SRCDIR}lib/afs/libcom_err.a ${utilib} $(deslib)
	${CC} ${LDFLAGS} -o rebuild rebuild.o kautils.o \
		${SRCDIR}lib/afs/libcom_err.a ${utilib} $(deslib) ${XLIBS} ${cmdlib}
	${INSTALL} -f rebuild ${DESTDIR}etc/kadb_check

system: install

${DESTDIR}root.server/usr/afs/bin/kaserver: kaserver
	${INSTALL} -s $? $@

${DESTDIR}etc/kas ${DESTDIR}root.server/usr/afs/bin/kas: kas 
	${INSTALL} -s $? $@

${DESTDIR}bin/kpwvalid ${DESTDIR}etc/kpwvalid \
${DESTDIR}root.server/usr/afs/bin/kpwvalid: kpwvalid 
	${INSTALL} $? $@

${DESTDIR}bin/klog ${DESTDIR}root.server/usr/afs/bin/klog: klog 
	${INSTALL} -s $? $@

${DESTDIR}bin/klog.krb ${DESTDIR}root.server/usr/afs/bin/klog.krb: klog.krb 
	${INSTALL} -s $? $@

${DESTDIR}bin/knfs: knfs 
	${INSTALL} -s $? $@

${DESTDIR}bin/kpasswd: kpasswd 
	${INSTALL} -s $? $@

${DESTDIR}lib/afs/libkauth.a: libkauth.a 
	${INSTALL} $? $@

${DESTDIR}lib/afs/libkauth.krb.a: libkauth.krb.a 
	${INSTALL} $? $@

${DESTDIR}etc/kdb: kdb 
	${INSTALL} -s $? $@

ukinstall webinstall: ${UKSRCS} Kkauth.h Kkauth.xdr.c Kkauth.cs.c kauth.h
	set ${UKERNELDIR}afs; $(MKDIR_IF_NEEDED)
	set ${UKERNELDIR}afsint; $(MKDIR_IF_NEEDED)
	${INSTALL} Kkauth.h ${UKERNELDIR}afsint/kauth.h
	${INSTALL} Kkauth.xdr.c ${UKERNELDIR}afsint/kauth.xdr.c
	${INSTALL} Kkauth.cs.c ${UKERNELDIR}afsint/kauth.cs.c
	${INSTALL} ${UKSRCS} ${UKERNELDIR}afs


install: all ukinstall ${DESTDIR}root.server/usr/afs/bin/kaserver \
	${DESTDIR}etc/kas ${DESTDIR}etc/kpwvalid \
	${DESTDIR}root.server/usr/afs/bin/kas \
	${DESTDIR}root.server/usr/afs/bin/kpwvalid \
	${DESTDIR}bin/klog ${DESTDIR}bin/klog.krb \
	${DESTDIR}root.server/usr/afs/bin/klog \
	${DESTDIR}root.server/usr/afs/bin/klog.krb \
	${DESTDIR}bin/knfs ${DESTDIR}bin/kpasswd \
	${DESTDIR}bin/kpwvalid ${DESTDIR}lib/afs/libkauth.a \
	${DESTDIR}lib/afs/libkauth.krb.a ${DESTDIR}etc/kdb
	${INSTALL} kautils.h kauth.h kaport.h ${DESTDIR}include/afs





