
COMPONENT=rxtest
include ../../config/Makefile.${SYS_NAME}
OPTIMIZE=-g

INCDIRS= -I${DESTDIR}include -I.. 
LDIRS= -L.. -L${DESTDIR}lib -L${DESTDIR}lib/afs  
utilib=${DESTDIR}lib/afs/util.a
LIBS= ${DESTDIR}lib/afs/libcmd.a  ../librx.a ${DESTDIR}lib/liblwp.a \
	 ${utilib} ${DESTDIR}/lib/afs/libsys.a ${XLIBS}

CFLAGS = ${OPTIMIZE} ${INCDIRS} -DRXDEBUG ${LDIRS} ${LIBS} ${XCFLAGS}

RXTESTOBJS = testclient.o testserver.o kstest.o kctest.o

BASICINCLS =  ../rx_clock.h ../rx_queue.h ../rx_event.h

LINK = ${CC} ${CFLAGS} -o $@ $@.o ${LIBS}

TESTS = testclient testserver kstest kctest testqueue tableGen generator

TH_TESTS = th_testserver th_testclient

.IGNORE: 

system all: test

test tests: ${TESTS}

th_test th_tests: ${TH_TESTS}

testclient: ../librx.a testclient.o
	-${LINK}

testserver: ../librx.a testserver.o
	-${LINK}

kstest: ../librx.a kstest.o
	-${LINK}

kctest: ../librx.a kctest.o
	-${LINK}

# th_ versions only have link lines for Solaris now.
th_testclient: th_testclient.o ../../libafsrpc/libafsrpc.so 
	${CC} ${CFLAGS} -mt -o th_testclient th_testclient.o \
		../../libafsrpc/libafsrpc.so -lpthread

th_testclient.o: testclient.c
	${CC} ${CFLAGS} -DAFS_PTHREAD_ENV -mt -c -o th_testclient.o \
		testclient.c

th_testserver: th_testserver.o ../../libafsrpc/libafsrpc.so 
	${CC} ${CFLAGS} -mt -o th_testserver th_testserver.o \
		../../libafsrpc/libafsrpc.so -lpthread

th_testserver.o: testserver.c
	${CC} ${CFLAGS} -DAFS_PTHREAD_ENV -mt -c -o th_testserver.o \
		testserver.c

testqueue: ../librx.a testqueue.o
	-${LINK}

${RXTESTOBJS}: ${BASICINCLS} ../rx.h

clean:
	rm -f *.o *.a ${TESTS} ${TH_TESTS} core
