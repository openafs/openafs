# Copyright 2000, International Business Machines Corporation and others.
# All Rights Reserved.
# 
# This software has been released under the terms of the IBM Public
# License.  For details, see the LICENSE file in the top-level source
# directory or online at http://www.openafs.org/dl/license10.html

# Makefile for gtx, a display-independent, object-oriented window toolkit.


SHELL=/bin/sh
COMPONENT=gtx
include ../config/Makefile.${SYS_NAME}

COMPILE_ET=$(SRCDIR)bin/compile_et
CFLAGS= ${DBUG} -I. -I${SRCDIR}include -I${SRCDIR}include/afs ${XCFLAGS}
LDFLAGS = ${XLDFLAGS}
#RPCINCLS=${SRCDIR}include/lwp.h ${SRCDIR}include/rx/rx.h bosint.h
RPCINCLS=
INCLS=gtxobjects.h \
	gtxwindows.h \
	gtxcurseswin.h \
	gtxinput.h \
	gtxdumbwin.h \
	gtxX11win.h \
	gtxobjects.h \
	gtxkeymap.h \
	gtxlightobj.h \
	gtxtextobj.h \
	gtxframe.h \
	gtxobjdict.h \
	${RPCINCLS} \
	${SRCDIR}include/afs/keys.h \
	${SRCDIR}include/afs/cellconfig.h \
	${SRCDIR}include/afs/cmd.h

LIBS= \
	${SRCDIR}lib/librx.a \
	${SRCDIR}lib/liblwp.a \
	${SRCDIR}lib/afs/libsys.a \
	${SRCDIR}lib/afs/libcmd.a \
	${SRCDIR}lib/afs/libkauth.a \
	${SRCDIR}lib/afs/libauth.a \
	${SRCDIR}lib/librxkad.a \
	${SRCDIR}lib/libdes.a \
	${SRCDIR}lib/afs/libcom_err.a \
	${SRCDIR}lib/afs/util.a

EXTRA_LIBS= ${TXLIBS} ${SRCDIR}lib/afs/libtermlib.a  ${XLIBS} 

LINT_LIBS = /usr/lib/lint/llib-lcurses.ln

KEYMAP_SRCS=keymap.c input.c frame.c

KEYMAP_OBJS=keymap.o input.o frame.o

WINDOW_SRCS=curseswindows.c \
	dumbwindows.c \
	X11windows.c \
	windows.c

WINDOW_OBJS=curseswindows.o \
	dumbwindows.o \
	X11windows.o \
	windows.o

OBJECT_SRCS=objects.c \
	lightobject.c \
	textobject.c

OBJECT_OBJS=objects.o \
	lightobject.o \
	textobject.o

DICT_SRCS=objdict.c
DICT_OBJS=objdict.o

CB_SRCS=textcb.c
CB_OBJS=textcb.o

#ERROBJS=err.o
ERRSRCS=
ERROJBS=

include ../config/Makefile.version

noversion: install

all: libgtx.a gtxtest

tests: object_test screen_test curses_test cb_test gtxtest

# All objects in the library
LIBSRCS = ${KEYMAP_SRCS} ${WINDOW_SRCS} ${OBJECT_SRCS} ${DICT_SRCS} \
	${ERRSRCS} ${CB_SRCS}
LIBOBJS = $(KEYMAP_OBJS) ${WINDOW_OBJS} ${OBJECT_OBJS} ${DICT_OBJS} \
	${ERROBJS} ${CB_OBJS}

${LIBSRCS}: ${INCLS}
$(LIBOBJS): ${INCLS}

err.c: err.et
	rm -f err.h err.c; $(COMPILE_ET) err -h err

keymap.o: keymap.c ${INCLS}

frame.o: frame.c $(INCLS)

input.o: input.c ${INCLS}

curseswindows.o: curseswindows.c ${INCLS}

dumbwindows.o: dumbwindows.c ${INCLS}

X11windows.o: X11windows.c ${INCLS}

windows.o: windows.c ${INCLS}

lightobject.o: lightobject.c ${INCLS}

textobject.o: textobject.c ${INCLS}

textcb.o: textcb.c ${INCLS}

#
# Test programs.
#
curses_test.o: curses_test.c ${INCLS} AFS_component_version_number.o

curses_test: curses_test.o $(LIBS)
	${CC} ${CFLAGS} -o curses_test curses_test.o $(LIBS) ${EXTRA_LIBS}

curses_test.lint: curses_test.c $(LINT_LIBS)
	lint ${CFLAGS} curses_test.c ${LINT_LIBS}

screen_test.o: screen_test.c ${INCLS} AFS_component_version_number.o

screen_test: screen_test.o ${WINDOW_OBJS} ${LIBS}
	${CC} ${CFLAGS} -o screen_test screen_test.o ${WINDOW_OBJS} ${LIBS} ${EXTRA_LIBS}

screen_test.lint: screen_test.o ${WINDOW_SRCS} ${LINT_LIBS}
	lint ${CFLAGS} screen_test.c ${WINDOW_SRCS} ${LINT_LIBS}

cb_test.o: cb_test.c ${INCLS} AFS_component_version_number.o

cb_test: cb_test.o ${CB_OBJS} ${LIBS}
	${CC} ${CFLAGS} -o cb_test cb_test.o ${CB_OBJS} ${LIBS} ${EXTRA_LIBS}

cb_test.lint: cb_test.c ${CB_SRCS} ${LINT_LIBS}
	lint ${CFLAGS} cb_test.c ${CB_SRCS} ${LINT_LIBS}

gtxtest.o: gtxtest.c ${INCLS} AFS_component_version_number.o

gtxtest: gtxtest.o libgtx.a ${LIBS}
	${CC} ${LDFLAGS} -o gtxtest gtxtest.o libgtx.a ${LIBS} ${EXTRA_LIBS}

gtxtest.lint: gtxtest.c llib-lgtx.ln ${LINT_LIBS}
	lint $(CFLAGS) gtxtest.c llib-lgtx.ln ${LINT_LIBS}

object_test.o: object_test.c ${INCLS} AFS_component_version_number.o

object_test: object_test.o ${WINDOW_OBJS} ${DICT_OBJS} ${OBJECT_OBJS} ${CB_OBJS} ${LIBS}
	${CC} ${CFLAGS} -o object_test object_test.o ${WINDOW_OBJS} ${DICT_OBJS} ${OBJECT_OBJS} ${CB_OBJS} ${LIBS} ${EXTRA_LIBS}

object_test.lint: object_test.c ${WINDOW_SRCS} ${DICT_SRCS} ${OBJECT_SRCS} ${CB_SRCS} ${LINT_LIBS}
	lint ${CFLAGS} object_test.c ${WINDOW_SRCS} ${DICT_SRCS} ${OBJECT_SRCS} ${CB_SRCS} ${LINT_LIBS}

#
# Misc others
#
system: install

libgtx.a: ${LIBOBJS} AFS_component_version_number.o
	-rm -f $@
	ar rv $@ ${LIBOBJS} AFS_component_version_number.o
	${RANLIB} $@

llib-lgtx.ln: ${LIBSRCS}
	lint -Cgtx ${CFLAGS} ${LIBSRCS}

lint: llib-lgtx.ln \
	curses_test.lint \
	screen_test.lint \
	cb_test.lint \
	gtxtest.lint \
	object_test.lint

install: all
	${INSTALL} gtx*.h ${DESTDIR}include/afs
	${INSTALL} libgtx.a ${DESTDIR}lib/afs

clean:
	rm -f  *.o *.a *.ln object_test screen_test curses_test err.c core cb_test gtxtest AFS_component_version_number.c
