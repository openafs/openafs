#
# $Locker:  $
#
# Makefile for EXPORT kernel extension, and friends
#

COMPONENT=export
include ../config/Makefile.${SYS_NAME}

  INSTALL = ${SRCDIR}bin/install
KERNELDIR = ../libafs/
UKERNELDIR = ../libuafs/
     DEFS =
   CFLAGS = ${OPTMZ} ${DEFS} ${INCS}
  EXPORTS = -bexport:export.exp
  IMPORTS = /lib/kernex.exp /lib/syscalls.exp extras.exp
     KOBJ = export.o symtab.o

include ../config/Makefile.version

all: export.ext export.ext.nonfs cfgexport cfgafs

noversion system: install

install: all kinstall ukinstall
	${INSTALL} export.ext ${DESTDIR}root.client/usr/vice/etc/dkload
	${INSTALL} export.ext.nonfs ${DESTDIR}root.client/usr/vice/etc/dkload
	${INSTALL} export.exp ${DESTDIR}lib/afs/export.exp
	${INSTALL} extras.exp ${DESTDIR}lib/afs/extras.exp
	${INSTALL} cfgexport  ${DESTDIR}root.client/usr/vice/etc/dkload/cfgexport
	${INSTALL} cfgafs     ${DESTDIR}root.client/usr/vice/etc/dkload/cfgafs

kinstall:
	${INSTALL} export.h ${DESTDIR}include
	${INSTALL} export.h ../libafs
	${INSTALL} export.exp ${DESTDIR}lib
	${INSTALL} export.exp ../libafs

ukinstall webinstall:
	${INSTALL} export.h ${UKERNELDIR}
	${INSTALL} export.exp ${UKERNELDIR}

export.ext: ${KOBJ}
	${LD} -o export.ext -eexport ${IMPORTS} ${KOBJ} ${EXPORTS} -lcsys

export.ext.nonfs: export.nonfs.o symtab.o
	${LD} -o export.ext.nonfs -eexport ${IMPORTS} export.nonfs.o symtab.o ${EXPORTS} -lcsys

export.nonfs.o:
	cc -DAFS_NONFSTRANS -c export.c
	-mv export.o export.nonfs.o

cfgexport: cfgexport.o
	cc -O -o cfgexport cfgexport.o

cfgafs: cfgafs.o
	cc -O -o cfgafs cfgafs.o

cfgexport.o: cfgexport.c AFS_component_version_number.c
cfgafs.o: cfgafs.c AFS_component_version_number.c

clean:
	rm -f *.o *.Zlst *.map *.out cfgexport cfgafs *.ext AFS_component_version_number.c
