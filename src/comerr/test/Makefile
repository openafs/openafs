
SRCDIR=DEST/
DESTDIR=DEST/
BACKDIR=../../../../PARENT/dest/
OPTIMIZE=-O

INCDIRS= -I${DESTDIR}include -I.. -I${BACKDIR}include
LDIRS= -L${DESTDIR}lib -L${DESTDIR}lib/afs -L.. -L${BACKDIR}lib -L${BACKDIR}lib/afs
utilib=${DESTDIR}lib/afs/util.a
LIBS= -lcom_err

CFLAGS = ${OPTIMIZE} ${INCDIRS} ${LDIRS} ${LIBS} ${XCFLAGS}

system:	test 

clean:
	rm -f *.o *.a test core

test:	test.o test1.o test2.o 
	$(CC)  ${CFLAGS} -o test test.o test1.o test2.o 
test.o:	test1.h test2.h
test1.o : test1.c
test1.c : test1.et
test2.o : test2.c
test2.c : test2.et

kinstall:
	@echo No kernel source here.

clean:
	rm -f *~ \#* *.bak \
		*.otl *.aux *.toc *.PS *.dvi *.x9700 *.ps \
		*.cp *.fn *.ky *.log *.pg *.tp *.vr \
		*.o libcom_err.a \
		com_err.o compile_et \
		et.ar TAGS y.tab.c lex.yy.c error_table.c \
		et_lex.lex.c \
		test1.h test1.c test2.h test2.c test \
		eddep makedep core

