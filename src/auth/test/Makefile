
include ../../config/Makefile.${SYS_NAME}

SRCDIR=
DESTDIR=DEST/
OPTIMIZE=-O

INCDIRS= -I./ -I${DESTDIR}include
LDIRS= -L${DESTDIR}lib -L${DESTDIR}lib/afs -L${DESTDIR}lib/rx
utilib=${DESTDIR}lib/afs/util.a
LIBS= -lauth -lsys -lrx -llwp ${utilib} ${XLIBS}

CFLAGS = ${OPTIMIZE} ${INCDIRS} ${LDIRS} ${XCFLAGS}

tests system all: testcellconf ktctest

testcellconf: testcellconf.o 
	$(CC) $(CFLAGS) -o testcellconf testcellconf.o  ${LIBS}

ktctest: ktctest.o 
	$(CC) $(CFLAGS) -o ktctest ktctest.o  ${LIBS}

clean:
	rm -f *.o *.a copyauth testcellconf ktctest setkey auth.h cellconfig.h acfg_errors.c ktc_errors.c core
