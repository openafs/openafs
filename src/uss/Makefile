#
# uss Makefile
#
# Copyright (C) 1991, 1990 Transarc Corporation - All rights reserved


SHELL=/bin/sh
COMPONENT=uss
include ../config/Makefile.${SYS_NAME}

INSTALL = ${SRCDIR}bin/install

#
# This makefile creates the following things:
#
PROGRAMS = uss

#
# and here's what it uses to do so:
#
LIBS =	${SRCDIR}lib/afs/libvolser.a \
	${SRCDIR}lib/afs/libvldb.a  \
	${SRCDIR}lib/afs/libkauth.a \
	${SRCDIR}lib/afs/libprot.a \
	${SRCDIR}lib/libubik.a \
	${SRCDIR}lib/afs/libauth.a \
	${SRCDIR}lib/librxkad.a \
	${SRCDIR}lib/afs/libsys.a \
	${SRCDIR}lib/libdes.a \
	${SRCDIR}lib/librx.a \
	${SRCDIR}lib/afs/libsys.a \
	${SRCDIR}lib/liblwp.a \
	${SRCDIR}lib/afs/libcmd.a \
	${SRCDIR}lib/afs/libcom_err.a\
	${SRCDIR}lib/afs/util.a \
	${XLIBS}

CFLAGS = ${DBUG} -I/usr/include -I${SRCDIR}include -I. ${XCFLAGS}
OBJS =  uss_procs.o \
	uss_common.o \
	uss_vol.o \
	uss_acl.o \
	uss_ptserver.o \
	uss_kauth.o \
	uss_fs.o \
	lex.yy.o \
	y.tab.o 

include ../config/Makefile.version

#
# Top-level options:
#
all :  ${PROGRAMS}

install: all
	${INSTALL} uss ${DESTDIR}etc

system: install

clean:
	rm -f *.o ${PROGRAMS} y.tab.c y.tab.h lex.yy.c core AFS_component_version_number.c

###### uss program ######

uss: uss.o  ${OBJS} 
	${CC} ${CFLAGS} -o uss uss.o ${OBJS} ${LIBS}

uss.o:  uss.c AFS_component_version_number.c
	 ${CC} -c ${CFLAGS} uss.c

###### common stuff ######

uss_procs.o : uss_procs.c
	${CC} -c ${CFLAGS} uss_procs.c

uss_common.o : uss_common.c
	${CC} -c ${CFLAGS} uss_common.c

uss_vol.o : uss_vol.c
	${CC} -c ${CFLAGS} uss_vol.c

uss_acl.o : uss_acl.c
	${CC} -c ${CFLAGS} uss_acl.c

uss_ptserver.o : uss_ptserver.c
	${CC} -c ${CFLAGS} uss_ptserver.c

uss_kauth.o : uss_kauth.c
	${CC} -c ${CFLAGS} uss_kauth.c

uss_fs.o : uss_fs.c
	${CC} -c ${CFLAGS} uss_fs.c

lex.yy.o : lex.yy.c y.tab.c
	${CC} -c ${CFLAGS} lex.yy.c

lex.yy.c : lex.c
	${LEX} lex.c

y.tab.o : y.tab.c
	${CC} -c ${CFLAGS} y.tab.c

uss.c: uss_common.h uss_procs.h uss_kauth.h uss_fs.h

uss_procs.c : uss_procs.h uss_common.h uss_acl.h

uss_common.c : uss_common.h

uss_vol.c : uss_vol.h uss_common.h uss_fs.h

uss_acl.c : uss_acl.h uss_common.h uss_fs.h

uss_ptserver.c: uss_ptserver.h

uss_kauth.c: uss_kauth.h uss_common.h

uss_fs.c: uss_fs.h

y.tab.c : grammar.y
	${YACC} -d grammar.y
