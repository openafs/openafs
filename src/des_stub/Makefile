#
# Copyright (C) 1997 by Transarc Corporation - All rights reserved
#
# Install the des libraries and header files from the 
# PARENT link instead of building them.
#

SHELL  = /bin/sh
include ../config/Makefile.${SYS_NAME}

RM     = /bin/rm
UKERNELDIR=../libuafs/
INSTALL=${SRCDIR}bin/install
SRC    =.
DESPAR =../../../DESLIB/dest/
DESINC =${DESPAR}include/
MKDIR_IF_NEEDED=[ -d $$1 ] || mkdir -p $$1
all:

kinstall:
	@echo No $@ source here.
 
ukinstall webinstall: ${DESINC}des.h ${DESINC}mit-cpyright.h
	set ${UKERNELDIR}des; $(MKDIR_IF_NEEDED)
	${INSTALL} ${DESINC}des.h ${UKERNELDIR}des/des.h
	${INSTALL} ${DESINC}mit-cpyright.h ${UKERNELDIR}des/mit-cpyright.h
	${INSTALL} ${DESPAR}lib/libdes.a ${UKERNELDIR}des/libdes.a

install install.noversion noversion system: ukinstall
	echo "Installing des stubs from ${DESPAR}:" `cd ${DESPAR};/bin/pwd`
	${RM} -f ${DESTDIR}lib/libdes.a
	${INSTALL} ${DESPAR}lib/libdes.a ${DESTDIR}lib/libdes.a
	${INSTALL} ${DESINC}des.h ${DESINC}des_conf.h ${DESINC}mit-cpyright.h ${DESTDIR}include
	${INSTALL} ${DESINC}des_odd.h ${DESTDIR}include/des_odd.h

clean:

lint:

