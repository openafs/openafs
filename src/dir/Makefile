
#
# (C) COPYRIGHT IBM CORPORATION 1987
# LICENSED MATERIALS - PROPERTY OF IBM
#
SHELL = /bin/sh
COMPONENT=dir
include ../config/Makefile.${SYS_NAME}

KERNELDIR = ../libafs/
UKERNELDIR = ../libuafs/
INSTALL = ${SRCDIR}bin/install
MKDIR_IF_NEEDED=[ -d $$1 ] || mkdir -p $$1

INCDIRS= -I${DESTDIR}include ${XINCLS}
CFLAGS = ${OPTMZ} ${INCDIRS} ${XCFLAGS}

OBJS=buffer.o dir.o salvage.o
include ../config/Makefile.version

system noversion: install

cmdall: all test

all: libdir.a

libdir.a: ${OBJS} AFS_component_version_number.o
	rm -f libdir.a
	ar rv libdir.a $(OBJS) AFS_component_version_number.o
	$(RANLIB) libdir.a

kinstall:
	${INSTALL} dir.h ${KERNELDIR}afs
	${INSTALL} dir.c ${KERNELDIR}afs/afs_dir.c

ukinstall webinstall:
	set ${UKERNELDIR}afs; $(MKDIR_IF_NEEDED)
	${INSTALL} dir.h ${UKERNELDIR}afs
	${INSTALL} dir.c ${UKERNELDIR}afs/afs_dir.c

install:	all kinstall ukinstall
	${INSTALL} libdir.a ${DESTDIR}lib/afs
	${INSTALL} dir.h ${DESTDIR}include/afs

test: 
	(cd test; $(MAKE) )

buffer.o: buffer.c

dir.o: dir.c dir.h

salvage.o: salvage.c dir.h

#doc:
#	${INSTALL} -m 644 dir.vdoc ${DESTDIR}doc/vdoc

clean:
	rm -f *.o *.a core AFS_component_version_number.c

beancount:
	wc -l *.c *.h

