
SRCDIR=DEST/
DESTDIR=DEST/
BACKDIR=../../../../PARENT/dest/
OPTIMIZE=-O

INCDIRS= -I${DESTDIR}include -I.. -I${BACKDIR}include
LDIRS= -L${DESTDIR}lib -L${DESTDIR}lib/afs -L.. -L${BACKDIR}lib -L${BACKDIR}lib/afs
utilib=${DESTDIR}lib/afs/util.a
LIBS= -lacl -lprot -lubik -lrx -llwp -lauth -lrxkad -lsys ${XLIBS}

CFLAGS = ${OPTIMIZE} ${INCDIRS} ${LDIRS} ${LIBS} ${XCFLAGS}

system:	acltest

clean:
	rm -f *.o *.a acltest core

acltest: acltest.o
	$(CC) -o acltest acltest.o $(CFLAGS)

acltest.o: acltest.c 

kinstall:
	@echo No kernel source here.


