# /* Copyright (C) 1990, 1989 Transarc Corporation - All rights reserved */

KERNELDIR=../libafs
UKERNELDIR=../libuafs
SHELL=/bin/sh
COMPONENT=config
include Makefile.${SYS_NAME}

MKDIR_IF_NEEDED=[ -d $$1 ] || mkdir -p $$1

CFLAGS=-g -I${DESTDIR}include ${XCFLAGS}

noversion: install.noversion

KDIRS = ${KERNELDIR}/afs ${KERNELDIR}/rx ${KERNELDIR}/afsint \
	${DESTDIR}include/afs ${KERNELDIR}/config

UKDIRS = ${UKERNELDIR}/afs ${UKERNELDIR}/rx ${UKERNELDIR}/afsint \
	 ${UKERNELDIR}/config

noversionMakef: $(KDIRS)
	-if [ ! -f Makefile.version ] ; \
	then \
		$(CP) Makefile.version-NOCML Makefile.version; \
		$(MAKE) -f Makefile.version  AFS_component_version_number.c; \
	fi; 

version: ${DESTDIR}bin/mkvers Makefile.version
	$(MAKE) -f Makefile.version  AFS_component_version_number.c

${DESTDIR}bin/mkvers: mkvers.c
	set ${DESTDIR}bin; $(MKDIR_IF_NEEDED)
	$(CC) -o ${DESTDIR}bin/mkvers mkvers.c

Makefile.version: Makefile.version-CML Makefile.version-NOCML
	$(RM) -f Makefile.version
	set -x ; \
	if	[ -r SRC/../CML/state ] ; \
	then	$(CP) Makefile.version-CML Makefile.version ; \
	else	$(CP) Makefile.version-NOCML Makefile.version ; \
	fi

all: config

mc.o: mc.c

config.o: config.c AFS_component_version_number.c

config: config.o mc.o
	$(SPEW) $(CC) $(CFLAGS) -o config config.o mc.o

param.xxx:

$(KDIRS): 
	set $@; $(MKDIR_IF_NEEDED)

${KERNELDIR}/afs/AFS_component_version_number.c: AFS_component_version_number.c
	-$(RM) -f $@; $(CP) $? $@

${KERNELDIR}/afs/param.h ${DESTDIR}include/afs/param.h :: param.${SYS_NAME}.h
	-$(RM) -f $@; $(CP) $? $@

${KERNELDIR}/afs/afs_sysnames.h ${DESTDIR}include/afs/afs_sysnames.h :: afs_sysnames.h
	$(RM) -f $@; $(CP) $? $@

DESTINCLS = ${DESTDIR}include/afs/afs_args.h ${DESTDIR}include/afs/venus.h ${DESTDIR}include/afs/debug.h ${DESTDIR}include/afs/afs_sysnames.h  ${DESTDIR}include/afs/param.h  ${DESTDIR}include/afs/stds.h  ${DESTDIR}include/afs/icl.h ../permit_xprt.h

${KERNELDIR}/afs/stds.h ${DESTDIR}include/afs/stds.h :: stds.h
	$(RM) -f $@; $(CP) $? $@

${KERNELDIR}/config/Makefile.${SYS_NAME} :: Makefile.${SYS_NAME}
	$(RM) -f $@; $(CP) $? $@

${KERNELDIR}/afs/icl.h ${DESTDIR}include/afs/icl.h :: icl.h 
	$(RM) -f $@; $(CP) $? $@

${KERNELDIR}/afs/afs_args.h ${DESTDIR}include/afs/afs_args.h :: afs_args.h 
	$(RM) -f $@; $(CP) $? $@

${DESTDIR}include/afs/venus.h :: venus.h 
	$(RM) -f $@; $(CP) $? $@

${DESTDIR}include/afs/debug.h :: debug.h 
	$(RM) -f $@; $(CP) $? $@

${KERNELDIR}/afs/permit_xprt.h ../permit_xprt.h:
	$(RM) -f ${KERNELDIR}/afs/permit_xprt.h ../permit_xprt.h
	if [ -f SRC/../xprt/permit_xprt.h ] ; then \
	   echo "XPRT" ; \
	   perm_src=`pwd`/../xprt ; \
	   echo "perm_src='$$perm_src'" ; \
	   set -x ; \
	   ln -s $$perm_src/permit_export.h ${KERNELDIR}/afs/permit_xprt.h ; \
	   ln -s $$perm_src/permit_xprt.h   ../permit_xprt.h ; \
	else \
	   echo "CONFIG" ; \
	   perm_src=`pwd` ; \
	   echo "perm_src='$$perm_src'" ; \
	   set -x ; \
	   ln -s $$perm_src/permit_xprt.h   ${KERNELDIR}/afs/permit_xprt.h ; \
	   ln -s $$perm_src/permit_xprt.h   ../permit_xprt.h ; \
	fi		     

$(UKDIRS): 
	set $@; $(MKDIR_IF_NEEDED)

${UKERNELDIR}/afs/AFS_component_version_number.c: AFS_component_version_number.c
	-$(RM) -f $@; $(CP) $? $@

${UKERNELDIR}/afs/param.h :: param.${SYS_NAME}_usr.h
	-$(RM) -f $@; $(CP) $? $@

${UKERNELDIR}/afs/afs_sysnames.h :: afs_sysnames.h
	$(RM) -f $@; $(CP) $? $@

${UKERNELDIR}/afs/stds.h :: stds.h
	$(RM) -f $@; $(CP) $? $@

${UKERNELDIR}/config/Makefile.${SYS_NAME} :: Makefile.${SYS_NAME}
	$(RM) -f $@; $(CP) $? $@

${UKERNELDIR}/afs/icl.h :: icl.h 
	$(RM) -f $@; $(CP) $? $@

${UKERNELDIR}/afs/venus.h :: venus.h 
	$(RM) -f $@; $(CP) $? $@

${UKERNELDIR}/afs/afs_args.h :: afs_args.h 
	$(RM) -f $@; $(CP) $? $@

${UKERNELDIR}/afs/permit_xprt.h:
	$(RM) -f ${UKERNELDIR}/afs/permit_xprt.h
	if [ -f SRC/../xprt/permit_xprt.h ] ; then \
	   echo "XPRT" ; \
	   perm_src=`pwd`/../xprt ; \
	else \
	   echo "CONFIG" ; \
	   perm_src=`pwd`/../config ; \
	fi; \
	echo "perm_src='$$perm_src'" ; \
	set -x ; \
	ln -s $$perm_src/permit_xprt.h   ${UKERNELDIR}/afs/permit_xprt.h

# lkversion is the target the Linux build uses to verify the build machine
# has headers in the right place for the requested kernel builds.
#
# To add AFS support to a new Linux kernel rev, see comments in the file
# ./linux-version.
#
lkversion:
	case ${SYS_NAME} in \
	*linux* ) \
		LINUX_SRCDIR="$(LINUX_SRCDIR)"; export LINUX_SRCDIR; \
		LINUX_VERS="$(LINUX_VERS)"; export LINUX_VERS; \
		sh ./linux-version ; \
	esac

install.noversion: $(KDIRS) noversionMakef all $(DESTINCLS) venus.h
	$(RM) -f ${DESTDIR}/include/afs/permit_xprt.h
	$(CP) permit_xprt.h ${DESTDIR}/include/afs/

system: install
install: version install.noversion kinstall ukinstall

# these are needed to compile the kernel.  Config is necessary to
# convert the MakefileProto in libafs and the kernel links provide the
# kernel include environment.  param.h is, well, param.h.  The afs_sysnames.h
# file is needed by param.h to create unique identifiers for each SYS_TYPE.

kinstall: lkversion config $(KDIRS) ${KERNELDIR}/afs/param.h \
	${KERNELDIR}/afs/stds.h \
	${KERNELDIR}/afs/permit_xprt.h ${KERNELDIR}/afs/afs_sysnames.h \
	${KERNELDIR}/config/Makefile.${SYS_NAME} \
	${KERNELDIR}/afs/afs_args.h \
	${KERNELDIR}/afs/icl.h \
	${KERNELDIR}/afs/AFS_component_version_number.c

ukinstall webinstall: $(DESTINCLS) config $(UKDIRS) ${UKERNELDIR}/afs/param.h ${UKERNELDIR}/afs/stds.h \
	${UKERNELDIR}/afs/permit_xprt.h ${UKERNELDIR}/afs/afs_sysnames.h \
	${UKERNELDIR}/config/Makefile.${SYS_NAME} \
	${UKERNELDIR}/afs/afs_args.h \
	${UKERNELDIR}/afs/icl.h \
	${UKERNELDIR}/afs/venus.h \
	${UKERNELDIR}/afs/AFS_component_version_number.c

clean:
	$(RM) -f *.o config core xprt 
