# Copyright 2000, International Business Machines Corporation and others.
# All Rights Reserved.
# 
# This software has been released under the terms of the IBM Public
# License.  For details, see the LICENSE file in the top-level source
# directory or online at http://www.openafs.org/dl/license10.html

# afsd makefile

SHELL = /bin/sh
DESTDIR = DEST/
SRCDIR = DEST/

SOURCES = afsd.c sys.c

COMPONENT=afsd
include ../config/Makefile.${SYS_NAME}
include ../config/Makefile.version

INSTALLro = ${INSTALL} -m 644
INSTALLex = ${INSTALL} -m 755
CFLAGS = -g -I${SRCDIR}include ${XCFLAGS}
LDFLAGS = -g ${XLDFLAGS}

noversion: install

#
# What to make
#
all:	afsd vsys

#
# Programs
#

AFSLIBS=$(SRCDIR)lib/afs/libauth.a ${SRCDIR}lib/afs/libcmd.a $(SRCDIR)lib/afs/libsys.a $(SRCDIR)lib/afs/util.a $(SRCDIR)lib/librx.a $(SRCDIR)lib/liblwp.a $(SRCDIR)lib/afs/util.a 

afsd: afsd.o $(AFSLIBS)
	case ${SYS_NAME} in \
	sgi_61 | sgi_62 | sgi_63) \
		${CC} ${CFLAGS} -g -o afsd afsd.o $(NON_SHARED) $(LDFLAGS) $(AFSLIBS) ${XLIBS} /usr/lib/libdwarf.a /usr/lib/libelf.a ;; \
	sgi_64 | sgi_65 ) \
		${CC} ${CFLAGS} -g -o afsd afsd.o $(NON_SHARED) $(LDFLAGS) $(AFSLIBS) ${XLIBS} /usr/lib32/libdwarf.a /usr/lib32/libelf.a ;; \
	* ) \
		${CC} ${CFLAGS} -g -o afsd afsd.o $(NON_SHARED) $(LDFLAGS) $(AFSLIBS) ${XLIBS} ;; \
	esac

vsys: vsys.o
	$(CC) -g -o vsys vsys.o ${SRCDIR}lib/afs/libsys.a $(LDFLAGS) ${XLIBS}

#
# Objects
#
afsd.o: afsd.c AFS_component_version_number.c

vsys.o: vsys.c AFS_component_version_number.c

clean:
	rm -f *.o vsys afsd core AFS_component_version_number.c

beancount:
	wc -l afsd.c

system: install

install: all
	${INSTALL} vsys ${DESTDIR}etc
	${INSTALL} afsd ${DESTDIR}root.client/usr/vice/etc
	case ${SYS_NAME} in \
		rs_aix*)  \
			${INSTALL} -f rc.afs.rs_aix ${DESTDIR}root.client/usr/vice/etc/dkload/rc.afs ; \
			${INSTALL} -f rc.dkload.client.rs_aix ${DESTDIR}root.client/usr/vice/etc/dkload/rc.dkload.client ; \
			${INSTALL} -f rc.dkload.server.rs_aix ${DESTDIR}root.client/usr/vice/etc/dkload/rc.dkload.server ;; \
		sgi_65 ) \
			${INSTALL} -f afs.rc.sgi_ipnos ${DESTDIR}root.client/usr/vice/etc/sgiload/afs.rc ;\
			${INSTALL} -f afs.rc.sgi_ipnos ${DESTDIR}root.server/etc/init.d/afs.rc ;;\
		sgi_* ) \
			${INSTALL} -f afs.rc.sgi ${DESTDIR}root.client/usr/vice/etc/sgiload/afs.rc ;\
			${INSTALL} -f afs.rc.sgi ${DESTDIR}root.server/etc/init.d/afs.rc ;\
			case ${SYS_NAME} in \
				sgi_53 | sgi_61 ) \
				${INSTALL} -f afs_rtsymtab.pl.${SYS_NAME} ${DESTDIR}root.client/usr/vice/etc/sgiload/afs_rtsymtab.pl ;;\
			esac;; \
		sun4_52 | sun4_53 | sun4c_52 | sun4c_53 | sun4m_52 | sun4m_53 | sun4m_54 | sun4_54 | sun4c_54 | sunx86_54) \
			${INSTALL} -f afs.rc.solaris ${DESTDIR}root.client/usr/vice/etc/modload/afs.rc ;;\
		sun4x_57 ) \
			${INSTALL} -f afs.rc.solaris.2.7 ${DESTDIR}root.client/usr/vice/etc/modload/afs.rc ;;\
		sun4x_58 ) \
			${INSTALL} -f afs.rc.solaris.2.8 ${DESTDIR}root.client/usr/vice/etc/modload/afs.rc ;;\
                sun4x_5? ) \
                        ${INSTALL} -f afs.rc.solaris.2_5 ${DESTDIR}root.client/usr/vice/etc/modload/afs.rc ;;\
		alpha_osf* ) \
			${INSTALL} -f afs.rc.alpha ${DESTDIR}root.client/usr/vice/etc/dkload/afs.rc ;;\
		hp_ux10? | hp_ux11? ) \
			${INSTALL} -f afs.rc.hpux10 ${DESTDIR}root.client/usr/vice/etc/dkload/afs.rc ;\
			${INSTALL} -f afs.hpux102.driver ${DESTDIR}root.client/usr/vice/etc/afs.driver ;;\
		alpha_dux* ) \
			${INSTALL} -f afs.rc.alpha_dux40 ${DESTDIR}root.client/usr/vice/etc/dkload/afs.rc ;;\
                hp?00_ux10? ) \
			${INSTALL} -f afs.rc.hpux10 ${DESTDIR}root.client/usr/vice/etc/dkload/afs.rc ;;\
		*linux* ) \
			${INSTALLex} -f afs.rc.linux ${DESTDIR}root.client/usr/vice/etc/afs.rc; \
			${INSTALL} -f afs.conf.linux ${DESTDIR}root.client/usr/vice/etc/afs.conf ;; \
		ppc_darwin*) \
			mkdir -p ${DESTDIR}root.client/usr/vice/etc/afs.kext/Contents/MacOS ;\
			${INSTALL} -f afs.ppc_darwin.plist ${DESTDIR}root.client/usr/vice/etc/afs.kext/Contents/Info.plist ; \
			${INSTALLex} -f afs.rc.darwin ${DESTDIR}root.client/usr/vice/etc/afs.rc ; \
			${INSTALL} -f afs.rc.darwin.plist ${DESTDIR}root.client/usr/vice/etc/StartupParameters.plist ;; \
		* )  \
			echo No rc scripts installed for ${SYS_NAME} ;; \
	esac
