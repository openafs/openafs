#
# Copyright 1987 by the Massachusetts Institute of Technology.
#
# For copying and distribution information,
# please see the file <mit-cpyright.h>.
#
# Makefile for BSD DES library
#
# First define machine type, compiler, and object file suffix
# Could be for VAX or PC-DOS
#
# Some of these files are cross compiled on the vax for the pc8086,
# using the MIT LCS cross-compilation environment
# Others are always executed on the vax(host).
#
# A machine, e.g. Vax, pc8086, IBM experimental workstation, is 
# described in terms of defines for its word length
# (BITS32 or BITS16), byte ordering (LSBFIRST or MSBFIRST) and 
# operating system (BSDUNIX or CROSSMSDOS).
#
# Make sure these are properly defined for any new machine types.
# target machine for run-time code (may cross-compile),
# override as needed 

SHELL = /bin/sh
include ../config/Makefile.${SYS_NAME}

UKERNELDIR=../libuafs/
SRC	=.
MKDIR_IF_NEEDED=[ -d $$1 ] || mkdir -p $$1

UKSRCS=des.h mit-cpyright.h

K_INC	= ${SRCDIR}include
DBG	=
GPROF	=
# by default do not try to hide entry points
F_CC	= -c ${GPROF} ${DBG} ${XCFLAGS}
F_CPP	= -I${K_INC} -I.
MK_STR	= "	\$${CC} -I\$${SRC} \$${F_CPP} \$${F_CC}"
LDFLAGS = ${XLDFLAGS}

# dont use optimizer, since it pessimizes on uvax2,
# somehow -pg makes it faster.

TAGS	= /bin/ctags

DES_LIB  = libdes.a
DEP_LIBS = ${DES_LIB} ${NDES_LIB}
LIBS	 = ${DEP_LIBS} ${XLIBS}
LLIB	 = llib-ldes.ln

SRCHOST	= \
	${SRC}/make_keyperm.c \
	${SRC}/make_ip.c \
	${SRC}/make_fp.c \
	${SRC}/make_p.c \
	${SRC}/make_s.c \
	${SRC}/make_s_table.c \
	${SRC}/make_p_table.c \
	${SRC}/make_odd.c \
	${SRC}/misc.c

OBJHOST	= \
	make_keyperm.o \
	make_ip.o \
	make_fp.o \
	make_e.o \
	make_p.o \
	make_s.o \
	make_s_table.o \
	make_p_table.o \
	make_odd.o \
	misc.o

SRCTARG	= \
	${SRC}/key_sched.c \
	${SRC}/des.c \
	${SRC}/cbc_encrypt.c \
	${SRC}/read_pssword.c \
	${SRC}/pcbc_encrypt.c \
	${SRC}/cksum.c \
	${SRC}/quad_cksum.c \
	${SRC}/new_rnd_key.c \
	${SRC}/key_parity.c \
	${SRC}/weak_key.c \
	${SRC}/util.c \
	${SRC}/strng_to_key.c \
	${SRC}/debug_decl.c

OBJTARG	= \
	key_sched.o \
	des.o \
	cbc_encrypt.o \
	read_pssword.o \
	pcbc_encrypt.o \
	cksum.o \
	quad_cksum.o \
	key_parity.o \
	weak_key.o \
	new_rnd_key.o \
	util.o \
	strng_to_key.o \
	debug_decl.o

CFILES	= ${SRCHOST} ${SRCTARG}
OBJECTS	= ${OBJHOST} ${OBJTARG}

CODE	= ${CFILES} ${SRC}/Makefile

# Generated files
GFILES	= \
	key_perm.h \
	p_table.h \
	s_table.h \
	odd.h \
	p.c \
	s.c \
	fp.c \
	ip.c

# Programs to create generated files
GPROGS	= \
	make_keyperm \
	make_p_table \
	make_s_table \
	make_odd \
	make_e \
	make_p \
	make_s \
	make_fp \
	make_ip \
	misc

# Library component lists.

TARGOBJS = \
	des.o \
	cbc_encrypt.o \
	pcbc_encrypt.o \
	cksum.o \
	new_rnd_key.o \
	key_sched.o \
	debug_decl.o \
	quad_cksum.o \
	key_parity.o \
	weak_key.o \
	read_pssword.o \
	strng_to_key.o \
	misco.o \
	${CRYPT_OBJ} \
	util.o

COMPONENT=des
include ../config/Makefile.version

# This is required to get compiler flags, but not use CFLAGS.
AFS_component_version_number.o: AFS_component_version_number.c
	$(CC) ${XCFLAGS} -c AFS_component_version_number.c

noversion: install

all:	${GFILES} ${CFILES} ${DEP_LIBS}

test: 
	cd test; $(MAKE)

system: install

kinstall:
	@echo No $@ source here.

ukinstall webinstall: all ${UKSRCS}
	set ${UKERNELDIR}des; $(MKDIR_IF_NEEDED)
	${INSTALL} ${UKSRCS} ${UKERNELDIR}des
	${INSTALL} libdes.a ${UKERNELDIR}des/libdes.a

${DESTDIR}lib/libdes.a: libdes.a
	${INSTALL} $? $@

install install.noversion: all ukinstall ${DESTDIR}lib/libdes.a
	${INSTALL} des.h des_conf.h mit-cpyright.h ${DESTDIR}include
	${INSTALL} odd.h ${DESTDIR}include/des_odd.h

clean:
	rm -f ${OBJECTS} ${DEP_LIBS} ${PROGS} ${GPROGS} ${GFILES} AFS_component_version_number.c
	rm -f *.s *.o *.b core *~ *.com *.ld
	rm -f tags TAGS ${LLIB}

tags:	${CFILES}
	rm -f tags; ${TAGS} ${CFILES}

print:	${CODE}
	lpr -p ${CODE}

src:	${CODE}

${CODE}:
	(cd ${SRC}; co -q $@)

lint:	${LLIB}

${LLIB}: ${CFILES}
	lint -I${K_INC} ${CFILES}

${DES_LIB}: ${TARGOBJS} ${COMMONOBJS} AFS_component_version_number.o
	rm -f ${DES_LIB}
	ar r ${DES_LIB} ${TARGOBJS} ${COMMONOBJS} AFS_component_version_number.o
	$(RANLIB) ${DES_LIB}

#	host system stuff
make_ip: make_ip.o misc.o
	${CC} make_ip.o misc.o ${LDFLAGS} -o make_ip
make_fp: make_fp.o misc.o
	${CC} make_fp.o misc.o ${LDFLAGS} -o make_fp
make_odd: make_odd.o misc.o
	${CC} ${LDFLAGS} make_odd.o -o make_odd misc.o
make_s_table: make_s_table.o misc.o	
	${CC} ${LDFLAGS} make_s_table.o misc.o -o make_s_table
make_p_table: make_p_table.o misc.o	
	${CC} ${LDFLAGS} make_p_table.o misc.o -o make_p_table
make_keyperm:	make_keyperm.o misc.o
	${CC} make_keyperm.o misc.o ${LDFLAGS} -o make_keyperm
make_e:	make_e.o misc.o
	${CC} make_e.o misc.o ${LDFLAGS} -o make_e
make_p:	make_p.o misc.o
	${CC} make_p.o misc.o ${LDFLAGS} -o make_p
make_s:	make_s.o misc.o
	${CC} make_s.o misc.o ${LDFLAGS} -o make_s

odd.h:	make_odd
	-./make_odd odd.h
s_table.h: make_s_table
	-./make_s_table s_table.h
p_table.h: make_p_table
	-./make_p_table p_table.h
key_perm.h: make_keyperm
	-./make_keyperm key_perm.h
ip.c:	make_ip
	-./make_ip ip.c
p.c:	make_p
	-./make_p p.c
s.c:	make_s
	-./make_s s.c
fp.c:	make_fp
	-./make_fp fp.c

depend: ${CFILES} AFS_component_version_number.o
	-${CC} -M ${F_CPP} ${CFILES} | \
	sed -e ':loop' \
	-e 't loop' | \
	awk ' { if ($$1 != prev) { print rec; rec = $$0; prev = $$1; } \
	else { if (length(rec $$2) > 70) { print rec; rec = $$0; } \
		   else rec = rec " " $$2 } } \
	  END { print rec } ' > makedep
	for file in ${OBJECTS} ; do \
	echo "$$file:" >> makedep; \
	echo ${MK_STR} "\$${SRC}/`basename $$file .o`.c" >> makedep; \
	done
	echo '/^# DO NOT DELETE THIS LINE/+1,$$d' >eddep
	echo '$$r makedep' >>eddep
	echo 'w' >>eddep
	cp Makefile Makefile.bak
	ex - Makefile < eddep
	rm eddep makedep
	echo '# DEPENDENCIES MUST END AT END OF FILE' >> Makefile
	echo -n '# IF YOU PUT STUFF HERE IT WILL GO AWAY' >> Makefile
	echo ' (see make depend above)' >> Makefile
	make depend2 SRCDIR=${SRCDIR}

depend2: ${CFILES} ${GFILES}
	${CC} -M ${F_CPP} ${CFILES} | \
	sed -e ':loop' \
	-e 't loop' | \
	awk ' { if ($$1 != prev) { print rec; rec = $$0; prev = $$1; } \
	else { if (length(rec $$2) > 70) { print rec; rec = $$0; } \
		   else rec = rec " " $$2 } } \
	  END { print rec } ' > makedep
	for file in ${OBJECTS} ; do \
	echo "$$file:" >> makedep; \
	echo ${MK_STR} "\$${SRC}/`basename $$file .o`.c" >> makedep; \
	done
	echo '/^# DO NOT DELETE THIS LINE/+1,$$d' >eddep
	echo '$$r makedep' >>eddep
	echo 'w' >>eddep
	cp Makefile Makefile.bak
	ex - Makefile < eddep
	rm eddep makedep
	echo '# DEPENDENCIES MUST END AT END OF FILE' >> Makefile
	echo -n '# IF YOU PUT STUFF HERE IT WILL GO AWAY' >> Makefile
	echo ' (see make depend above)' >> Makefile

misc.o make_e.o: AFS_component_version_number.c
# DO NOT DELETE THIS LINE

make_keyperm.o: make_keyperm.c 
make_keyperm.o: 
make_ip.o: make_ip.c 
make_ip.o: tables.h
make_fp.o: make_fp.c 
make_fp.o: tables.h
make_p.o: make_p.c
make_p.o: conf.h tables.h
make_s.o: make_s.c ./s_table.h
make_s_table.o: make_s_table.c
make_s_table.o: tables.h
make_p_table.o: make_p_table.c 
make_p_table.o: conf.h
make_p_table.o: tables.h
make_odd.o: make_odd.c 
misc.o: misc.c des_internal.h
misc.o: conf.h
key_sched.o: key_sched.c
key_sched.o: conf.h 
key_sched.o: des.h des_internal.h
key_sched.o: mit-cpyright.h
key_sched.o: des_conf.h ./key_perm.h ./odd.h
key_test.o: key_test.c
key_test.o: des.h
key_test.o: mit-cpyright.h
key_test.o: des_conf.h
testit.o: testit.c
testit.o: des.h
testit.o: mit-cpyright.h
testit.o: des_conf.h
des.o: des.c des_internal.h
des.o: des.h
des.o: mit-cpyright.h
des.o: des_conf.h ./s_table.h ./p_table.h ./ip.c
des.o: ./p.c ./fp.c
cbc_encrypt.o: cbc_encrypt.c 
cbc_encrypt.o: des.h
cbc_encrypt.o: mit-cpyright.h
cbc_encrypt.o: des_conf.h
verify.o: verify.c 
verify.o: des.h
verify.o: mit-cpyright.h
verify.o: des_conf.h
read_pssword.o: read_pssword.c
read_pssword.o: des.h
read_pssword.o: mit-cpyright.h
read_pssword.o: des_conf.h 
pcbc_encrypt.o: pcbc_encrypt.c
pcbc_encrypt.o: des.h des_internal.h
pcbc_encrypt.o: mit-cpyright.h
pcbc_encrypt.o: des_conf.h
cksum.o: des.h
cksum.o: mit-cpyright.h
cksum.o: des_conf.h
quad_cksum.o: quad_cksum.c 
quad_cksum.o: des.h
quad_cksum.o: mit-cpyright.h
quad_cksum.o: des_conf.h
#random_key.o: random_key.c 
#random_key.o: des.h
#random_key.o: mit-cpyright.h
#random_key.o: des_conf.h 
key_parity.o: key_parity.c des.h odd.h
weak_key.o: weak_key.c des.h des_internal.h
new_rnd_key.o: des.h des_internal.h 
util.o: util.c
util.o: des.h
util.o: mit-cpyright.h
util.o: des_conf.h
strng_to_key.o: strng_to_key.c 
strng_to_key.o: des.h
strng_to_key.o: mit-cpyright.h
strng_to_key.o: des_conf.h ./odd.h
debug_decl.o: debug_decl.c
make_keyperm.o:
	${CC} -I${SRC} ${F_CPP} ${F_CC} ${SRC}/make_keyperm.c
make_ip.o:
	${CC} -I${SRC} ${F_CPP} ${F_CC} ${SRC}/make_ip.c
make_fp.o:
	${CC} -I${SRC} ${F_CPP} ${F_CC} ${SRC}/make_fp.c
make_p.o:
	${CC} -I${SRC} ${F_CPP} ${F_CC} ${SRC}/make_p.c
make_s.o:
	${CC} -I${SRC} ${F_CPP} ${F_CC} ${SRC}/make_s.c
make_s_table.o:
	${CC} -I${SRC} ${F_CPP} ${F_CC} ${SRC}/make_s_table.c
make_p_table.o:
	${CC} -I${SRC} ${F_CPP} ${F_CC} ${SRC}/make_p_table.c
make_odd.o:
	${CC} -I${SRC} ${F_CPP} ${F_CC} ${SRC}/make_odd.c
misc.o:
	${CC} -I${SRC} ${F_CPP} ${F_CC} ${SRC}/misc.c
misco.c: misc.c
	-ln -s misc.c misco.c
misco.o: misco.c
	 ${CC} -I${SRC} ${F_CPP} ${F_CC} -o misco.o misco.c -DDONT_INCL_MAIN
key_sched.o:
	${CC} -I${SRC} ${F_CPP} ${F_CC} ${SRC}/key_sched.c
key_test.o:
	${CC} -I${SRC} ${F_CPP} ${F_CC} ${SRC}/key_test.c
testit.o:
	${CC} -I${SRC} ${F_CPP} ${F_CC} ${SRC}/testit.c
des.o:
	${CC} -I${SRC} ${F_CPP} ${F_CC} ${SRC}/des.c
cbc_encrypt.o:
	${CC} -I${SRC} ${F_CPP} ${F_CC} ${SRC}/cbc_encrypt.c
verify.o:
	${CC} -I${SRC} ${F_CPP} ${F_CC} ${SRC}/verify.c
read_pssword.o:
	${CC} -I${SRC} ${F_CPP} ${F_CC} ${SRC}/read_pssword.c
pcbc_encrypt.o:
	${CC} -I${SRC} ${F_CPP} ${F_CC} ${SRC}/pcbc_encrypt.c
cksum.o:
	${CC} -I${SRC} ${F_CPP} ${F_CC} ${SRC}/cksum.c
quad_cksum.o:
	${CC} -I${SRC} ${F_CPP} ${F_CC} ${SRC}/quad_cksum.c
#random_key.o:
#	${CC} -I${SRC} ${F_CPP} ${F_CC} ${SRC}/random_key.c
key_parity.o:
	${CC} -I${SRC} ${F_CPP} ${F_CC} ${SRC}/key_parity.c
weak_key.o:
	${CC} -I${SRC} ${F_CPP} ${F_CC} ${SRC}/weak_key.c
new_rnd_key.o:
	${CC} -I${SRC} ${F_CPP} ${F_CC} ${SRC}/new_rnd_key.c
util.o:
	${CC} -I${SRC} ${F_CPP} ${F_CC} ${SRC}/util.c
strng_to_key.o:
	${CC} -I${SRC} ${F_CPP} ${F_CC} ${SRC}/strng_to_key.c
debug_decl.o:
	${CC} -I${SRC} ${F_CPP} ${F_CC} ${SRC}/debug_decl.c
# DEPENDENCIES MUST END AT END OF FILE
# IF YOU PUT STUFF HERE IT WILL GO AWAY (see make depend above)
