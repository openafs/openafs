# Copyright 2000, International Business Machines Corporation and others.
# All Rights Reserved.
# 
# This software has been released under the terms of the IBM Public
# License.  For details, see the LICENSE file in the top-level source
# directory or online at http://www.openafs.org/dl/license10.html

SHELL=/bin/sh
COMPONENT=vol
include ../config/Makefile.${SYS_NAME}
include ../config/Makefile.version

INCDIRS= -I${TOP_SRCDIR}/config -I. -I${DESTDIR}include ${FSINCLUDES}
LDFLAGS = ${OPTMZ} ${PROF} ${LDIRS} ${XLDFLAGS}

LIBS= ${DESTDIR}lib/afs/libcmd.a vlib.a ${SRCDIR}lib/afs/util.a \
	${DESTDIR}lib/afs/libsys.a ${DESTDIR}lib/afs/libdir.a \
	${DESTDIR}lib/liblwp.a  ${DESTDIR}lib/afs/libacl.a ${XLIBS}

CFLAGS = -D${SYS_NAME} ${OPTMZ} ${INCDIRS} ${XCFLAGS} ${DBG} ${DBG_DEFS}

PROGS = salvager volinfo

PUBLICHEADERS=nfs.h vnode.h viceinode.h volume.h voldefs.h partition.h\
	fssync.h ihandle.h namei_ops.h

VLIBOBJS=vnode.o volume.o vutil.o partition.o fssync.o purge.o \
	 clone.o nuke.o devname.o listinodes.o common.o ihandle.o \
	 namei_ops.o


OBJECTS=${VLIBOBJS} physio.o vol-salvage.o vol-info.o 

all:	${DESTDIR}lib/afs/vlib.a \
	${DESTDIR}root.server/usr/afs/bin/salvager \
	${DESTDIR}root.server/usr/afs/bin/volinfo \
	gi

system: install

kinstall ukinstall webinstall:
	@echo No kernel source here.

${DESTDIR}root.server/usr/afs/bin/salvager: salvager 
	${INSTALL} -s $? $@

${DESTDIR}root.server/usr/afs/bin/volinfo: volinfo
	${INSTALL} -s $? $@

${DESTDIR}lib/afs/vlib.a: vlib.a
	${INSTALL} $? $@

$(DESTDIR)root.server/usr/afs/bin/fs_conv_dux40D:  fs_conv_dux40D
	   ${INSTALL} -s $? $@

$(DESTDIR)root.server/usr/afs/bin/xfs_size_check:  xfs_size_check
	   ${INSTALL} -s $? $@

$(DESTDIR)root.server/usr/afs/bin/fs_conv_sol26:  fs_conv_sol26
	   ${INSTALL} -s $? $@

install: all $(FS_CONV_OSF40D) $(XFS_SIZE_CHECK) $(FS_CONV_SOL26)
	${INSTALL} ${PUBLICHEADERS} ${DESTDIR}include/afs


${OBJECTS}: ${PUBLICHEADERS} ${SRCDIR}include/lwp.h ${SRCDIR}include/lock.h \
	 ${SRCDIR}include/afs/afsint.h vutils.h salvage.h\
	AFS_component_version_number.c

vol-salvage.o vutil.o: volinodes.h

vlib.a:	${VLIBOBJS} AFS_component_version_number.o
	rm -f vlib.a
	$(AR) r vlib.a ${VLIBOBJS} AFS_component_version_number.o
	$(RANLIB) vlib.a

# new salvager:  remove references to /vice by linking with novice.o
salvager: vol-salvage.o physio.o vlib.a
	${CC} ${LDFLAGS} -o salvager vol-salvage.o physio.o ${LIBS}

vol-salvage: vol-salvage.o
vol-info: vol-info.o physio.o ihandle.o

listinodes.o: listinodes.c AFS_component_version_number.c
	case ${SYS_NAME} in				\
		hp?00_ux101 | hp_ux10? )		\
			${CC} ${CFLAGS} -D_FILE64 -c listinodes.c \
				;;			\
		* )	${CC} ${CFLAGS} -c listinodes.c \
				;;			\
	esac

gi: ${DESTDIR}/lib/afs/libsys.a
	case ${SYS_NAME} in \
                *linux* | sgi_* | *fbsd* ) \
			echo "Don't build gi on ${SYS_NAME}";; \
                *) \
			${CC} ${CFLAGS} -c gi.c ; \
			${CC} ${LDFLAGS} -o gi gi.o ${DESTDIR}/lib/afs/libsys.a;; \
        esac

volinfo: vol-info.o physio.o ihandle.o
	case ${SYS_NAME} in \
		*linux*) \
			${CC} ${CFLAGS} ${DBG} -o volinfo vol-info.o physio.o \
				ihandle.o ${LIBS}  ;; \
		*) \
			${CC} ${CFLAGS} ${DBG} -o volinfo vol-info.o physio.o \
				 ihandle.o ${LIBS} ;; \
	esac


fs_conv_dux40D: fs_conv_411.o
	${CC} ${CFLAGS} ${DESTDIR}lib/afs/libcmd.a -o fs_conv_dux40D fs_conv_411.o  ${LIBS} 

fs_conv_sol26: fs_conv_411.o vlib.a 
	${CC} ${CFLAGS} ${DESTDIR}lib/afs/libcmd.a -o fs_conv_sol26 fs_conv_411.o  ${LIBS} 

fs_conv_411.o: fs_conv_411.c AFS_component_version_number.c
	${CC} ${CFLAGS} -c fs_conv_411.c

xfs_size_check: xfs_size_check.c
	${CC} ${CFLAGS} -o xfs_size_check xfs_size_check.c

clean:
	rm -f *.o *.a AFS_component_version_number.c
	rm -f ${SCMPROGS} ${STAGEPROGS} core

beancount:
	wc -l *.c *.h

test:
	cd test; $(MAKE)
