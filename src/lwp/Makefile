SHELL = /bin/sh
COMPONENT=lwp
include ../config/Makefile.${SYS_NAME}

INSTALL = ${SRCDIR}bin/install

include ../config/Makefile.${SYS_NAME}
# this is important code, so let's optimize it.  The optimizer aggravates any
# subtle bugs that may have been introduced in process.s, be warned!
OPTIMIZE=${LWP_OPTMZ}

CFLAGS=${OPTIMIZE} -I${SRCDIR}include ${XCFLAGS}

include ../config/Makefile.version

LIBOBJS=lwp.o process.o lock.o iomgr.o timer.o fasttime.o preempt.o \
	waitkey.o AFS_component_version_number.o
LIBTOBJS=tlwp.o process.o lock.o tiomgr.o timer.o fasttime.o preempt.o AFS_component_version_number.o pthread.o

noversion: install

#
#	The lwp library
#
liblwp.a	: ${LIBOBJS}
		-rm -f liblwp.a
		ar rv liblwp.a ${LIBOBJS}
		$(RANLIB) liblwp.a

libtlwp.a	: ${LIBTOBJS}
		-rm -f libtlwp.a
		ar rv libtlwp.a ${LIBTOBJS}
		$(RANLIB) libtlwp.a

tlwp.o: lwp.c lwp.h
	${CC} ${CFLAGS} -DUSE_SOLARIS_THREADSX -c lwp.c
	mv lwp.o tlwp.o

tiomgr.o: iomgr.c lwp.h
	${CC} ${CFLAGS} -DUSE_SOLARIS_THREADSX -c iomgr.c
	mv iomgr.o tiomgr.o

pthread.o	: pthread.c
	${CC} ${CFLAGS} -DUSE_SOLARIS_THREADSX -c pthread.c

process.s:
	# This is required so sgi_64+ does not try to build this file.
	echo Nothing to be done for process.s

process.o	: process.s process.c
			case "$(SYS_NAME)" in \
				pmax_he1) \
					$(CC) -c -I${SRCDIR}include process.s;; \
				sun4c_51 | sun4c_52 | sun4m_51 | sun4m_52 | sun4c_53 | sun4m_53  | sun4_53 | sun4_52 | sun4_54 | sun4c_54 | sun4m_54 | sun4x_5? | sunx86_54) \
					/usr/ccs/lib/cpp -P -I${SRCDIR}include process.s process.ss; \
					${AS} process.ss -o process.o; \
					rm process.ss ;; \
				sgi_5* ) \
					$(CC) -c -I${SRCDIR}include -KPIC -G0  process.s;; \
				sgi_61 | sgi_62 | sgi_63 ) \
					$(CC) -c ${XCFLAGS} -I${SRCDIR}include -KPIC -G0  process.s;; \
				hp* | i386_linux* | sgi_64 | sgi_65) \
					${CC} ${CFLAGS} -c process.c;; \
				ncrx86_*) \
					/usr/ccs/lib/cpp -P -I${SRCDIR}include process.s process.ss; \
					${AS} -o process.o process.ss; \
					rm process.ss ;; \
				alpha_osf1 | alpha_osf20 |  alpha_osf30 | alpha_osf32 | alpha_osf32c | alpha_dux?? ) \
					${AS} -v  -P -DOSF -I${SRCDIR}include -DASSEMBLER process.s; \
					${AS} -v process.i -o process.o;; \
				hp_ux110 ) \
					${CC} ${CFLAGS} -c process.c;; \
				*) \
					/lib/cpp -P -I${SRCDIR}include process.s process.ss; \
					${AS} process.ss -o process.o; \
					rm process.ss ;; \
			esac

iomgr.o		: iomgr.c lwp.h timer.h
		${CC} -c ${CFLAGS}  iomgr.c

preempt.o	: preempt.c lwp.h preempt.h
lwp.o		: lwp.c lwp.h
			case "$(SYS_NAME)" in \
				rs_aix*) \
					$(CC) -I${SRCDIR}include -c lwp.c;; \
				*)  \
					$(CC) ${CFLAGS} -c lwp.c;; \
			esac
lock.o		: lock.c lock.h lwp.h
timer.o		: timer.c
fasttime.o	: fasttime.c

exc_handling.o	: exc_handling.c

#
# tests
#
rw: rw.o liblwp.a DEST/lib/afs/util.a
	${CC} ${CFLAGS} -o rw rw.o liblwp.a DEST/lib/afs/util.a

rw.o: rw.c
	${CC} ${CFLAGS} -c rw.c


#
#	Installation
#
system:	install

kinstall ukinstall webinstall:
	@echo No kernel source here.

install: liblwp.a #libtlwp.a
		${INSTALL} liblwp.a ${DESTDIR}lib
		${INSTALL} *.h ${DESTDIR}include
		case ${SYS_NAME} in \
	            sun4_52 | sun4_53 | sun4c_52 | sun4c_53 | sun4m_52 | sun4m_53 | sun4_54 | sun4c_54 | sun4m_54 | sun4x_5?) \
			echo "Don't install libtrx.a for ${SYS_NAME}" ;; \
		     * ) \
			echo "Don't install libtrx.a for ${SYS_NAME}" ;; \
		esac
clean:
	rm -f *.o *.a core AFS_component_version_number.c

beancount:
	wc -l *.[ch]
