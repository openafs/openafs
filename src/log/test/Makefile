
SRCDIR=DEST/
DESTDIR=DEST/
BACKDIR=../../../../PARENT/dest/
OPTIMIZE=-O

INCDIRS= -I${DESTDIR}include -I.. -I${BACKDIR}include -I${DESTDIR}include/afs -I${BACKDIR}include/afs
LDIRS= -L${DESTDIR}lib -L${DESTDIR}lib/afs -L.. -L${BACKDIR}lib -L${BACKDIR}lib/afs
utilib=${DESTDIR}lib/afs/util.a
LIBS= -lauth -lrxkad -ldes -lsys -lrx -llwp -lcmd ${utilib} ${XLIBS}

CFLAGS = ${OPTIMIZE} ${INCDIRS} ${LDIRS} ${LIBS} ${XCFLAGS}

# top-level options
system all test: testlog gettoktest

lint:	
	lint  ${INCDIRS} testlog.c
	lint  ${INCDIRS} gettoktest.c

clean:	
	rm -f *.o testlog gettoktest core

testlog: testlog.o 
	${CC} -o testlog testlog.o ${CFLAGS}

gettoktest: gettoktest.o 
	${CC} -o gettoktest gettoktest.o ${CFLAGS}

