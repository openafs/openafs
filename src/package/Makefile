# (C) Copyright Transarc Corporation 1989
# Licensed Materials - Property of Transarc
# All Rights Reserved
#
# Makefile for package, the AFS workstation configuration tool.
#

SHELL    = /bin/sh
COMPONENT=package
include ../config/Makefile.${SYS_NAME}

INCLUDES = -I/usr/include -I${TOP_SRCDIR}/config -I${SRCDIR}include
DBGFLAG  = -UDEBUG
CFLAGS   = ${DBUG} -DKFLAG -DANDREW -DVICE $(DBGFLAG) $(INCLUDES) $(XCFLAGS)
YFLAGS   = -vd
#LFLAGS  = -ll
LINT     = lint -hp

HFILES = package.h \
	 validupdates.h \
	 systype.h \
	 globals.h

SRCS = package.c \
	conftree.c \
	check.c \
	update.c \
	fileops.c \
	messages.c \
	misc.c \
	globfuncs.c

TEST_SRCS = test.c \
	conftree.c \
	messages.c \
	misc.c \
	globfuncs.c

OTHERSRCS = gram.y

OBJS =  package.o \
	gram.o \
	lex.o \
	conftree.o \
	check.o \
	update.o \
	fileops.o \
	messages.o \
	misc.o \
	globfuncs.o

TEST_OBJS = test.o \
	test_gram.o \
	lex.o \
	conftree.o \
	messages.o \
	misc.o \
	globfuncs.o

LIBS =  ${DESTDIR}lib/afs/libsys.a \
        ${DESTDIR}lib/afs/libcmd.a \
	${DESTDIR}lib/afs/libcom_err.a \
        ${DESTDIR}lib/librx.a \
	${DESTDIR}lib/afs/libsys.a \
	${DESTDIR}lib/afs/util.a \
	${DESTDIR}lib/liblwp.a \
	${XLIBS}

include ../config/Makefile.version

all	: package package_test

package	: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(NON_SHARED) ${LIBS}

package_test : $(TEST_OBJS)
	$(CC) $(CFLAGS) -o package_test $(TEST_OBJS) ${LIBS}

package.o: package.c AFS_component_version_number.o
test.o: test.c AFS_component_version_number.o

lint	: $(SRCS) gram.c lex.c test_gram.c
	$(LINT) $(SRCS) gram.c lex.c test_gram.c
	/bin/rm gram.c lex.c test_gram.c

lex.c:
	-$(RM) -f lex.c
	$(LEX)  -t lex.l > lex.c

system	: install

clean	:
	rm -f *.o gram.c test_gram.c core package package_test *.BAK y.output yacc.tmp yacc.acts\
	AFS_component_version_number.c lex.c y.tab.h

install: package package_test
	$(INSTALL) -s  -c -m 755 package      ${DESTDIR}etc
	$(INSTALL) -s  -c -m 755 package_test ${DESTDIR}etc
	${INSTALL} -ns -c -m 755 makepkgfiles ${DESTDIR}etc
