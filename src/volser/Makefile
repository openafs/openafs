# Copyright 2000, International Business Machines Corporation and others.
# All Rights Reserved.
# 
# This software has been released under the terms of the IBM Public
# License.  For details, see the LICENSE file in the top-level source
# directory or online at http://www.openafs.org/dl/license10.html

SHELL=/bin/sh
COMPONENT=volser
include ../config/Makefile.${SYS_NAME}

COMPILE_ET=${SRCDIR}bin/compile_et
CFLAGS=${DBG} ${OPTMZ} -I${SRCDIR}include ${XCFLAGS} ${DBG_DEFS}
LDFLAGS=${DBG} ${OPTMZ} ${XLDFLAGS}

INCDIRS= -I${TOP_SRCDIR}/config -I${SRCDIR}include/afs -I${SRCDIR}include
INCLIBS=-L${SRCDIR}lib/afs -L${SRCDIR}lib

VINCLS=${SRCDIR}include/afs/partition.h ${SRCDIR}include/afs/volume.h \
	${SRCDIR}include/afs/vlserver.h vol.h dump.h volser.h  lockdata.h 

RINCLS=${SRCDIR}include/rx/rx.h ${SRCDIR}include/rx/xdr.h \
       ${SRCDIR}include/afs/keys.h ${SRCDIR}include/afs/cellconfig.h \
	${SRCDIR}include/ubik.h ${SRCDIR}include/afs/cmd.h

TARGETS= ${DESTDIR}include/afs/volser.h \
	 ${DESTDIR}include/afs/volint.h \
	 ${DESTDIR}etc/vos \
	 ${DESTDIR}root.server/usr/afs/bin/vos \
	 ${DESTDIR}root.server/usr/afs/bin/volserver \
	 ${DESTDIR}lib/afs/libvolser.a 

INTINCLS=volint.h volser.h 

HACKS=${SRCDIR}lib/afs/libdir.a

LIBS= \
	${SRCDIR}lib/afs/libaudit.a \
	${SRCDIR}lib/afs/vlib.a \
	${SRCDIR}/lib/afs/libacl.a \
	${SRCDIR}lib/afs/libsys.a \
	${SRCDIR}lib/afs/libvldb.a \
	${SRCDIR}lib/libubik.a \
	${SRCDIR}lib/afs/libauth.a \
	${SRCDIR}lib/afs/libcmd.a \
	${SRCDIR}lib/librxkad.a \
	${SRCDIR}lib/libdes.a \
	${SRCDIR}lib/librxstat.a \
	${SRCDIR}lib/librx.a \
	${SRCDIR}lib/liblwp.a \
	${SRCDIR}lib/afs/libsys.a \
	${SRCDIR}lib/afs/libcom_err.a \
	${SRCDIR}lib/afs/libkauth.a \
	${SRCDIR}lib/afs/libusd.a \
	${SRCDIR}lib/afs/util.a

VSOBJS=vsprocs.o vsutils.o lockprocs.o volint.xdr.o volerr.o 
SOBJS=volmain.o volprocs.o physio.o common.o voltrans.o volerr.o \
 volint.cs.o dumpstuff.o  volint.ss.o volint.xdr.o
COBJS=volint.cs.o volint.xdr.o

include ../config/Makefile.version

all: volserver libvolser.a vos restorevol

restorevol: restorevol.c
	${CC} -g ${CFLAGS} -o restorevol restorevol.c \
		${SRCDIR}lib/afs/libcmd.a ${SRCDIR}lib/afs/util.a
	${INSTALL} restorevol ${SRCDIR}etc

vos: vos.o ${VSOBJS} libvolser.a ${LIBS}
	${CC} ${LDFLAGS} -g -o vos vos.o $(VSOBJS) libvolser.a  ${LIBS} \
		${XLIBS}

libvolser.a: volint.cs.o  $(VSOBJS) volint.ss.o AFS_component_version_number.o
	-rm -f libvolser.a
	ar rv libvolser.a volint.cs.o $(VSOBJS) volint.ss.o \
		AFS_component_version_number.o 
	$(RANLIB) libvolser.a

volser.h volerr.c: volerr.et volser.p.h
	rm -f volser.h volerr.c; ${COMPILE_ET} volerr -h volser

volint.h volint.ss.c volint.cs.c volint.xdr.c: volint.xg
	${SRCDIR}bin/rxgen -x volint.xg

vsutils.o: vsutils.c ${VINCLS} ${RINCLS} ${INTINCLS}

volmain.o: volmain.c ${VINCLS} ${RINCLS} AFS_component_version_number.c
volprocs.o: volprocs.c ${VINCLS} ${RINCLS} ${INTINCLS}
dumpstuff.o: dumpstuff.c ${VINCLS} ${RINCLS} ${INTINCLS}
voldump.o: voldump.c ${VINCLS} ${RINCLS}
vos.o: vos.c ${VINCLS} ${RINCLS} ${INTINCLS} AFS_component_version_number.c
vsprocs.o: vsprocs.c ${VINCLS} ${RINCLS} ${INTINCLS}
physio.o: physio.c ${VINCLS}
common.o: common.c ${VINCLS}
lockprocs.o: lockprocs.c ${VINCLS} ${INTINCLS} ${RINCLS}

volint.cs.o: volint.cs.c ${INTINCLS}
volint.ss.o: volint.ss.c ${INTINCLS}
volint.xdr.o: volint.xdr.c ${INTINCLS}

volserver: $(SOBJS) $(LIBS) $(HACKS)
	case ${SYS_NAME} in \
                *linux* | *fbsd* ) \
                        ${CC} ${DBUG} -o volserver $(SOBJS) $(HACKS) \
				${LDFLAGS} $(LIBS) ${XLIBS} ;; \
                *) \
                        ${CC} ${DBUG} -g -o volserver $(SOBJS) $(HACKS) \
				${LDFLAGS} $(LIBS) ${XLIBS} ;; \
        esac

noversion system: install

${DESTDIR}include/afs/volser.h :: volser.h
	${INSTALL} $? $@

${DESTDIR}include/afs/volint.h :: volint.h
	${INSTALL} $? $@

${DESTDIR}etc/vos ${DESTDIR}root.server/usr/afs/bin/vos :: vos
	${INSTALL} $? $@

${DESTDIR}lib/afs/libvolser.a :: libvolser.a
	${INSTALL} $? $@

${DESTDIR}root.server/usr/afs/bin/volserver :: volserver
	${INSTALL} $? $@

install: all $(TARGETS)

clean:
	rm -f volserver *.o volint.ss.c volint.cs.c *.a  volint.h \
	volint.xdr.c vos core volser.h volerr.c AFS_component_version_number.c restorevol

lint:	vos.c vsprocs.c volmain.c volprocs.c lockprocs.c
	lint -hb ${INCDIRS} ${INCLIBS} vos.c
	lint -hb ${INCDIRS} ${INCLIBS} vsprocs.c
	lint -hb  ${INCDIRS} ${INCLIBS} volmain.c
	lint -hb ${INCDIRS} ${INCLIBS}  volprocs.c
	lint -hb ${INCDIRS} ${INCLIBS} lockprocs.c
