SRCDIR=/usr/andy/
LIBRX=${SRCDIR}lib/librx.a
DESTDIR=/usr/andy/
INSTALL=${SRCDIR}bin/install
LIBS=${LIBRX} ${SRCDIR}lib/liblwp.a
CFLAGS=-g -I.. -I. -I${SRCDIR}include -DDEBUG ${XCFLAGS}

all: sample_client sample_server

sample_client: sample_client.o ${LIBS} sample.cs.o
	${CC} ${CFLAGS} -o sample_client sample_client.o sample.cs.o ${LIBS}

sample_server: sample_server.o sample.ss.o ${LIBS}
	${CC} ${CFLAGS} -o sample_server sample_server.o sample.ss.o ${LIBS}

sample_client.o:	sample.h
sample_server.o:	sample.h
sample.cs.c sample.ss.c sample.er.c sample.h: sample.xg
	rxgen sample.xg
