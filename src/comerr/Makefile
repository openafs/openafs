#
# Makefile for error-table routines
#
# Copyright 1987, 1989 MIT Student Information Processing Board
# For copyright info, see mit-sipb-cr.h.
#
#	$Locker:  $
#

SHELL = /bin/sh
COMPONENT=comerr
include ../config/Makefile.${SYS_NAME}
MKDIR_IF_NEEDED=[ -d $$1 ] || mkdir -p $$1

LINTFLAGS= -uhvpb
LINTFILES= error_msg.c et_name.c com_err.c
LIBOBJS= error_msg.o et_name.o com_err.o

UKERNELDIR=../libuafs/

CFLAGS= ${OPTMZ} -I${DESTDIR}include -I${TOP_SRCDIR}/config ${XCFLAGS}
FILES=	Makefile et_name.c error_msg.c compile_et.c \
		error_table.y et_lex.lex.l \
		com_err.c com_err.h \
		error_table.h mit-sipb-cr.h \
		test.c test1.et test2.et \
		compiler.h internal.h

CFILES=	compile_et.c error_table.c error_msg.c et_name.c \
	com_err.c

UKSRCS=com_err.c com_err.h error_msg.c error_table.h mit-sipb-cr.h internal.h et_name.c

COMPONENT=comerr
include ../config/Makefile.version

utilib=${SRCDIR}lib/afs/util.a ${XLIBS}

system noversion: install

#
# what to build...
#
${DESTDIR}bin/compile_et: compile_et
	${INSTALL} compile_et ${DESTDIR}bin/compile_et

all:	libcom_err.a ${DESTDIR}bin/compile_et

lint:	llib-lcom_err.ln

archive:	et.tar

doc:	com_err.dvi

#
# rules
#
.SUFFIXES: .l .h .c .et .ps .x9700 .mss .dvi .texinfo

.l.c:
	$(RM) -f $*.c
	$(LEX) -t $*.l >$*.c

.et.c:
	./compile_et $*.et

.et.h:
	./compile_et $*.et

.texinfo.dvi:
	tex $<

.dvi.ps:
	rm -f $@.new
	dvi2ps -r $< > $@.new
	mv $@.new $@

.c.o:
#	${CC} -c -pg ${CFLAGS} $*.c
#	mv $*.o profiled/$*.o
	${CC} -c ${CFLAGS} $*.c

#
# real entries...
#
compile_et:	compile_et.o error_table.o
	case $(SYS_NAME) in \
	*_linux* ) \
	${CC} ${CFLAGS} -o compile_et compile_et.o error_table.o ${utilib};; \
	* ) \
	${CC} ${CFLAGS} -o compile_et compile_et.o error_table.o ${utilib} -ll;; \
	esac

et.tar:	${FILES}
	rm -f et.tar
	tar cfrlv et.tar ${FILES}

tags:	TAGS

kinstall:
	@echo No $@ source here.

ukinstall webinstall: install
	set ${UKERNELDIR}afs; $(MKDIR_IF_NEEDED)
	${INSTALL} ${UKSRCS} ${UKERNELDIR}afs

install: all ukinstall
	${INSTALL} com_err.h ${DESTDIR}include/afs/com_err.h
	${INSTALL} error_table.h ${DESTDIR}include/afs/error_table.h
	${INSTALL} mit-sipb-cr.h ${DESTDIR}include/afs/mit-sipb-cr.h
	${INSTALL} libcom_err.a ${DESTDIR}lib/afs/libcom_err.a

TAGS:	et_name.c error_msg.c compile_et.c error_table.c \
		lex.yy.c perror.c
	etags et_name.c error_msg.c compile_et.c \
		error_table.c perror.c

libcom_err.a:	$(LIBOBJS) AFS_component_version_number.o
	- rm -f libcom_err.a
	$(AR) cruv libcom_err.a $(LIBOBJS) AFS_component_version_number.o
	$(RANLIB) libcom_err.a

llib-lcom_err.ln: $(LINTFILES)
	lint -Ccom_err $(LINTFLAGS) $(LINTFILES)

clean:
	-rm -f *~ \#* *.bak \
		*.otl *.aux *.toc *.PS *.dvi *.x9700 *.ps \
		*.cp *.fn *.ky *.log *.pg *.tp *.vr \
		*.o libcom_err.a \
		com_err.o compile_et \
		et.ar TAGS y.tab.c lex.yy.c error_table.c \
		et_lex.lex.c \
		test1.h test1.c test2.h test2.c test \
		eddep makedep core\
		AFS_component_version_number.c

test:
	cd test; $(MAKE)

# 'make depend' code
depend: ${CFILES} et_lex.lex.c
	rm -f eddep makedep
	${CC} -M ${CFLAGS} ${CFILES} | \
	sed 's; ./; ;' | \
	sed -e ':loop' \
	    -e 's/\.\.\/[^ /]*\/\.\./../' \
	    -e 't loop' > makedep
	echo '/^# DO NOT DELETE THIS LINE/+1,$$d' >eddep
	echo '$$r makedep' >>eddep
	echo 'w' >>eddep
	cp Makefile Makefile.bak
	ed - Makefile < eddep
	rm eddep makedep

compile_et.o:  AFS_component_version_number.c
#
# the last line in the makefile should be...
# DO NOT DELETE THIS LINE -- make depend depends on it.
compile_et.o: compile_et.c 
compile_et.o: mit-sipb-cr.h
error_table.o: error_table.c
error_table.o: internal.h
error_table.o: mit-sipb-cr.h
error_table.o: error_table.h
error_table.o: mit-sipb-cr.h
error_table.o: et_lex.lex.c
error_msg.o: error_msg.c
error_msg.o: error_table.h
error_msg.o: mit-sipb-cr.h
error_msg.o: internal.h
error_msg.o: mit-sipb-cr.h
et_name.o: et_name.c
et_name.o: error_table.h
et_name.o: mit-sipb-cr.h
et_name.o: internal.h
et_name.o: mit-sipb-cr.h
perror.o: perror.c
perror.o: internal.h
perror.o: mit-sipb-cr.h
perror.o: com_err.h
perror.o: mit-sipb-cr.h
perror.o: mit-sipb-cr.h
com_err.o: com_err.c
com_err.o: error_table.h
com_err.o: mit-sipb-cr.h
com_err.o: internal.h
com_err.o: mit-sipb-cr.h
