SRCDIR=
DESTDIR=DEST/
BACKDIR=../../../../PARENT/dest/
OPTIMIZE=-O

INCDIRS= -I${DESTDIR}include -I.. -I${BACKDIR}include
LDIRS= -L${DESTDIR}lib -L${DESTDIR}lib/afs -L.. -L${BACKDIR}lib -L${BACKDIR}lib/afs
utilib=${DESTDIR}lib/afs/util.a

CFLAGS = ${OPTIMIZE} ${INCDIRS} ${LDIRS} ${LIBS} ${XCFLAGS}

system:	testproc smail-notifier

testproc: testproc.c 
	$(CC) $(CFLAGS) -o testproc testproc.c

smail-notifier: smail-notifier.c 
	$(CC) $(CFLAGS) -o smail-notifier smail-notifier.c

clean:
	rm -f  *.a *.o testproc smail-notifier core

