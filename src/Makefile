# Copyright 2000, International Business Machines Corporation and others.
# All Rights Reserved.
# 
# This software has been released under the terms of the IBM Public
# License.  For details, see the LICENSE file in the top-level source
# directory or online at http://www.openafs.org/dl/license10.html

SHELL=/bin/sh
DESTDIR=dest/
SRCDIR=${DESTDIR}
INSTALL=${SRCDIR}bin/install
WASHTOOL=${SRCDIR}bin/washtool

#WASHTOOLRCSUPDATE=${WASHTOOL} rcsupdate
WASHTOOLCMD=$(MAKE)
WASHTOOLFLAGS=-argument install

COMPILE_PART1=${WASHTOOL} ${WASHTOOLCMD}
COMPILE_PART2=${WASHTOOLFLAGS}

MKDIR_IF_NEEDED=[ -d $$1 ] || mkdir -p $$1

# Enable build+install of obsolete and insecure packages
# Set to anything other than YES, or comment out to disable the build
WITH_DEPRECATED=YES

# To compile AFS from scratch in the src tree run "make SYS_NAME=<type>".
#   This recursively calls "make install ..." and does not depend on the
#   existence of any non-standard programs.
# To compile a package and all its predecessors using washtool run
#   "make <package>".  This requires that @sys works, and that washtool,
#   rcsupdate and our version of ci and co exist.
# To compile for the kernel type "make blibafs".  This assumes washtool,
#   install, rxgen, compile_et and others already exist and runs it with
#   "-argument kinstall".

all:
	set ${SYS_NAME}/dest; $(MKDIR_IF_NEEDED)
	$(MAKE) install "COMPILE_PART1=cd obj; cd" "COMPILE_PART2=; $(MAKE) install SYS_NAME=${SYS_NAME} DESTDIR=`pwd`/${SYS_NAME}/dest/ SRCDIR=`pwd`/${SYS_NAME}/dest/" "COMPILE_PART3=; $(MAKE) test SYS_NAME=${SYS_NAME} DESTDIR=`pwd`/${SYS_NAME}/dest/ SRCDIR=`pwd`/${SYS_NAME}/dest/" SYS_NAME=${SYS_NAME} "WASHTOOL=echo No washtool: "

basics:
	-if test -f "${WASHTOOL}"; \
	 then \
		${WASHTOOL} project . . -noretarget; \
		${WASHTOOL} destdir; \
	fi
#	test -d ${DESTDIR}

minproject: basics
	${WASHTOOL} project . . -noretarget
	${WASHTOOL} destdir

project: basics cmd comerr minproject   # try and initialize project

config: basics
	${COMPILE_PART1} config ${COMPILE_PART2}

pinstall: basics config
	${COMPILE_PART1} pinstall ${COMPILE_PART2}

procmgmt: basics config pinstall
	${COMPILE_PART1} procmgmt ${COMPILE_PART2}

util: basics pinstall procmgmt
	${COMPILE_PART1} util ${COMPILE_PART2}

audit: basics pinstall rx rxkad fsint
	${COMPILE_PART1} audit ${COMPILE_PART2}

comerr: basics util
	${COMPILE_PART1} comerr ${COMPILE_PART2}

cmd: basics comerr
	${COMPILE_PART1} cmd ${COMPILE_PART2}
	@case ${SYS_NAME} in \
	sgi_6* | sun4x_57 | sun4x_58 | hp_ux11* | sparc64_linux* | alpha_linux* ) \
		${COMPILE_PART1} cmd ${COMPILE_PART2} install64 ;; \
	esac

lwp: project util
	${COMPILE_PART1} lwp ${COMPILE_PART2}

rxgen: minproject pinstall
	${COMPILE_PART1} rxgen ${COMPILE_PART2}

rxobj:
	${WASHTOOL} object rx

kernrx: 
	${COMPILE_PART1} rx ${COMPILE_PART2}

rx: project lwp rxgen
	${COMPILE_PART1} rx ${COMPILE_PART2}

rxstat: rx
	${COMPILE_PART1} rxstat ${COMPILE_PART2}

rxincls: rxobj
	${COMPILE_PART1} rx ${COMPILE_PART2}

rxtests: rxobj
	${COMPILE_PART1} rx ${COMPILE_PART3}

fsint: project kfsint

kfsint: rxgen rxincls lwp
	${COMPILE_PART1} fsint ${COMPILE_PART2}

export2: project fsint

export: minproject fsint
	@case ${SYS_NAME} in \
	rs_aix*)	\
	    ${COMPILE_PART1} export ${COMPILE_PART2} ;; \
	  * ) echo skipping export for ${SYS_NAME} ;; \
	esac

afs: minproject export
	${COMPILE_PART1} afs ${COMPILE_PART2}

des: minproject config
	if [ -f "obj/des/Makefile" ] ; then \
	   ${COMPILE_PART1} des ${COMPILE_PART2} ; \
	else \
	   ${COMPILE_PART1} des_stub ${COMPILE_PART2} ; \
	fi

sys: project des rx rxstat fsint
	${COMPILE_PART1} sys ${COMPILE_PART2}

rxkad: project des
	${WASHTOOL} object rxkad
	${WASHTOOL} object rxkad/domestic
	${COMPILE_PART1} rxkad ${COMPILE_PART2}

kernauth: 
	${COMPILE_PART1} auth ${COMPILE_PART2}

auth: project comerr des lwp rx sys rxkad audit
	${COMPILE_PART1} auth ${COMPILE_PART2}

ubik: project auth 
	${COMPILE_PART1} ubik ${COMPILE_PART2}

#	cd obj/ubik; $(MAKE) test SYS_NAME=${SYS_NAME} 
ubiktests: ubik sys
	${COMPILE_PART1} ubik ${COMPILE_PART3}

ptserver: project ubik cmd comerr auth audit
	${COMPILE_PART1} ptserver ${COMPILE_PART2}

kauth: project ubik cmd auth comerr ptserver audit libacl
	${COMPILE_PART1} kauth ${COMPILE_PART2}

dauth: project ubik cmd auth comerr
	${COMPILE_PART1} dauth ${COMPILE_PART2}

libacl: project ptserver
	${COMPILE_PART1} libacl ${COMPILE_PART2}

kerndir: minproject 
	${COMPILE_PART1} dir ${COMPILE_PART2}

dir: project libacl
	${COMPILE_PART1} dir ${COMPILE_PART2}

# Removed from sgi_* case below:
# ${COMPILE_PART1} sgiefs ${COMPILE_PART2};;
sgiefs:
	case ${SYS_NAME} in \
	sgi_*) \
                echo "SGI efs not supported" ;; \
	*) \
		echo No efs stuff for ${SYS_NAME};; \
	esac

vol: project dir afs sgiefs
	${COMPILE_PART1} vol ${COMPILE_PART2}

vlserver: project vol audit
	${COMPILE_PART1} vlserver ${COMPILE_PART2}

viced: project vlserver audit
	${COMPILE_PART1} viced  ${COMPILE_PART2}

tviced: project viced vlserver libafsrpc libafsauthent
	case ${SYS_NAME} in \
	alpha_dux*|sgi_*|sun4x_*|rs_aix*|*linux*|hp_ux110) \
		${COMPILE_PART1} tviced  ${COMPILE_PART2} ;; \
	*) \
		echo Not building MT viced for ${SYS_NAME} ;; \
	esac

volser: project tviced usd kauth audit
	${COMPILE_PART1} volser ${COMPILE_PART2}

venus: project volser ptserver
	${COMPILE_PART1} venus ${COMPILE_PART2}
	${COMPILE_PART1} venus/test ${COMPILE_PART2}

afsd: project sys kauth
	${COMPILE_PART1} afsd ${COMPILE_PART2}

null: project
	${COMPILE_PART1} null ${COMPILE_PART2}

gtx: project null auth # kauth rxkad ?
	case ${SYS_NAME} in \
	  rs_aix* | sun4x_5? | sgi_6? | *linux* | ppc_darwin* | *fbsd*) \
		rm -f ${DESTDIR}lib/afs/libtermlib.a; \
		ln -s libnull.a ${DESTDIR}lib/afs/libtermlib.a; \
		${COMPILE_PART1} gtx ${COMPILE_PART2};; \
	  *) \
		rm -f ${DESTDIR}lib/afs/libtermlib.a; \
		ln -s /usr/lib/libtermlib.a ${DESTDIR}lib/afs/libtermlib.a; \
		${COMPILE_PART1} gtx ${COMPILE_PART2};; \
	esac

fsprobe: project util fsint
	${COMPILE_PART1} fsprobe ${COMPILE_PART2}

scout: project gtx fsprobe
	${COMPILE_PART1} scout ${COMPILE_PART2}

uss: des kauth project rx vlserver vol
	${COMPILE_PART1} uss ${COMPILE_PART2}

bozo: project ntp audit
	${COMPILE_PART1} bozo ${COMPILE_PART2}


# Removed from respective case statements below:
# ${COMPILE_PART1} vfsckaix42 ${COMPILE_PART2} ;;
# ${COMPILE_PART1} vfscksol25 ${COMPILE_PART2} ;;
# ${COMPILE_PART1} vfscksol26 ${COMPILE_PART2} ;;
# ${COMPILE_PART1} vfscksol27 ${COMPILE_PART2} ;;
vfsck:	minproject vol
	set -x; \
	case ${SYS_NAME} in \
	sgi_* | *linux* | rs_aix42 | ppc_darwin* | hp_ux* | *fbsd* ) \
		echo skip vfsck for ${SYS_NAME} ;; \
	 * ) \
		${COMPILE_PART1} vfsck ${COMPILE_PART2} ;; \
	esac

login: project kauth rxkad
	set -x; \
	case ${SYS_NAME} in \
	sgi_* ) \
		${COMPILE_PART1} sgistuff ${COMPILE_PART2} ;; \
	rs_aix42 ) \
		${WASHTOOL} object login ; \
		${COMPILE_PART1} tsm41 ${COMPILE_PART2} ;; \
	alpha_dux* ) \
		${COMPILE_PART1} sia ${COMPILE_PART2} ;; \
	sun4x_55 ) \
		 ${COMPILE_PART1} login ${COMPILE_PART2} ;; \
	sun4x_* | hp_ux11* | *linux* | *fbsd* ) \
		${COMPILE_PART1} pam ${COMPILE_PART2} ;; \
	ppc_darwin* ) \
		echo Skipping login for ${SYS_NAME} ;; \
	* ) \
		${COMPILE_PART1} login ${COMPILE_PART2} ;; \
	esac

KERNELDIR= \
  afs \
  dir \
  rx \
  fsint \
  auth \
  rxkad \
  util \
  libacl \
  vlserver \
  sgiefs  \
  rxstat


UKERNELDIR= \
  afs \
  dir \
  rx \
  fsint \
  util \
  auth \
  rxkad \
  vlserver \
  kauth \
  des \
  afsweb \
  rxstat


libafs: minproject libafs_setup ${KERNELDIR}
	${COMPILE_PART1} libafs ${COMPILE_PART2}

libuafs: minproject libuafs_setup ${UKERNELDIR}
	set -x; \
	case ${SYS_NAME} in \
	hp_ux102* ) \
		echo Skipping libuafs for ${SYS_NAME} ;; \
	* ) \
		${COMPILE_PART1} libuafs ${COMPILE_PART2} ;; \
	esac

afsweb: minproject kauth dauth 
	${COMPILE_PART1} afsweb ${COMPILE_PART2}

update: project auth
	${COMPILE_PART1} update ${COMPILE_PART2}

log: project auth 
	${COMPILE_PART1} log ${COMPILE_PART2}

bu_utils: project
	${COMPILE_PART1} bu_utils ${COMPILE_PART2}

usd: project
	${COMPILE_PART1} usd ${COMPILE_PART2}

bubasics: project comerr
	${COMPILE_PART1} bubasics ${COMPILE_PART2}

butm: project bubasics usd
	${COMPILE_PART1} butm ${COMPILE_PART2}

butc: project bubasics butm budb bucoord cmd
	${COMPILE_PART1} butc ${COMPILE_PART2}

tbutc: project bubasics butm budb bucoord cmd butc
	case ${SYS_NAME} in \
	alpha_dux*|sgi_*|sun4x_*|rs_aix4*|*linux*|hp_ux110) \
		${COMPILE_PART1} tbutc ${COMPILE_PART2} ;; \
	*) \
		echo Not building MT butc for ${SYS_NAME} ;; \
	esac

budb: project bubasics
	${COMPILE_PART1} budb ${COMPILE_PART2}

bucoord: project bubasics budb volser
	${COMPILE_PART1} bucoord ${COMPILE_PART2}

xstat: project
	${COMPILE_PART1} xstat ${COMPILE_PART2}

afsmonitor: project gtx xstat
	${COMPILE_PART1} afsmonitor ${COMPILE_PART2}

#
# Washtool internal targets
#
ircs: project
	case ${SYS_NAME} in \
	sgi_* ) \
		echo skip ircs for ${SYS_NAME} ;; \
	*) \
		${COMPILE_PART1} ircs ${COMPILE_PART2} ;; \
	esac

wash: project
	case ${SYS_NAME} in \
	sgi_* ) \
		echo skip wash for ${SYS_NAME} ;; \
	*) \
		${COMPILE_PART1} wash ${COMPILE_PART2} ;; \
	esac

tests: rxtests ubiktests

# pthread based user space RX library
libafsrpc: rx rxkad des
	case ${SYS_NAME} in \
	alpha_dux*|sgi_*|sun4x_*|rs_aix*|*linux*|hp_ux110) \
	${COMPILE_PART1} libafsrpc  ${COMPILE_PART2} ;; \
	*) \
		echo Not building MT libafsrpc for ${SYS_NAME} ;; \
	esac

libafsauthent: ubik auth kauth libafsrpc
	case ${SYS_NAME} in \
	alpha_dux*|sgi_*|sun4x_*|rs_aix*|*linux*|hp_ux110) \
	${COMPILE_PART1} libafsauthent  ${COMPILE_PART2} ;; \
	*) \
		echo Not building MT libafsrpc for ${SYS_NAME} ;; \
	esac

libadmin_real: 
	${COMPILE_PART1} libadmin ${COMPILE_PART2}
	${COMPILE_PART1} libadmin/adminutil ${COMPILE_PART2}
	${COMPILE_PART1} libadmin/vos ${COMPILE_PART2}
	${COMPILE_PART1} libadmin/client ${COMPILE_PART2}
	${COMPILE_PART1} libadmin/kas ${COMPILE_PART2}
	${COMPILE_PART1} libadmin/pts ${COMPILE_PART2}
	${COMPILE_PART1} libadmin/bos ${COMPILE_PART2}
	${COMPILE_PART1} libadmin/cfg ${COMPILE_PART2}
	${COMPILE_PART1} libadmin/test ${COMPILE_PART2}
	${COMPILE_PART1} libadmin/samples ${COMPILE_PART2}

libadmin: libafsauthent bozo
	case ${SYS_NAME} in \
	alpha_dux*|sgi_*|sun4x_*|rs_aix*|*linux*|hp_ux110) \
			$(MAKE) libadmin_real SYS_NAME=$(SYS_NAME) \
			WASHTOOL="$(WASHTOOL)" \
			COMPILE_PART1="$(COMPILE_PART1)" \
			COMPILE_PART2="$(COMPILE_PART2)" \
			SRCDIR=$(SRCDIR) DESTDIR=$(DESTDIR) \
			DBG_DEFS=$(DBG_DEFS) ;; \
	*) \
		echo Not building MT libadmin for ${SYS_NAME} ;; \
	esac

finale: project cmd comerr afsd allrcmds butc tbutc libafs libuafs audit kauth log package \
	ptserver scout bu_utils ubik uss bozo vfsck volser \
	venus update xstat afsmonitor dauth tests libafsrpc \
	libafsauthent libadmin
	${COMPILE_PART1} finale ${COMPILE_PART2}

# Use washtool to ensure MakefileProto is current and obj/libafs exists.

libafs_setup: config minproject export
# How about if no RCSUPDATE exists?
#   Well then you're going to lose.  Or, if you insist, set
#   WASHTOOLRCSUPDATE to be a no-op
# Warning: The next few lines are _extremely_ sensitive to spaces and tabs.
	${WASHTOOL} object libafs
	case ${SYS_NAME} in \
	alpha_dux* ) \
		MKAFS_OSTYPE=DUX;; \
	hp_ux* ) \
		MKAFS_OSTYPE=HPUX;; \
	*linux*) \
		MKAFS_OSTYPE=LINUX;; \
	*fbsd*) \
		MKAFS_OSTYPE=FBSD;; \
	rs_aix* ) \
		MKAFS_OSTYPE=AIX;; \
	sgi_* ) \
		MKAFS_OSTYPE=IRIX;; \
	sun4x_5* ) \
		MKAFS_OSTYPE=SOLARIS;; \
	ppc_darwin* ) \
		MKAFS_OSTYPE=DARWIN;; \
	* ) \
		echo WARNING: No MakefileProto for sys ${SYS_NAME} ; \
	esac; \
	obj/config/config src/libafs/MakefileProto.$$MKAFS_OSTYPE obj/libafs/Makefile ${SYS_NAME}

libuafs_setup: config minproject export
# How about if no RCSUPDATE exists?
#   Well then you're going to lose.  Or, if you insist, set
#   WASHTOOLRCSUPDATE to be a no-op
	${WASHTOOL} object libuafs
	case ${SYS_NAME} in \
	hp_ux* ) \
		MKAFS_OSTYPE=HPUX;; \
	*linux*) \
		MKAFS_OSTYPE=LINUX;; \
	*fbsd*) \
		MKAFS_OSTYPE=FBSD;; \
	rs_aix* ) \
		MKAFS_OSTYPE=AIX;; \
	sun4x_5* ) \
		MKAFS_OSTYPE=SOLARIS;; \
	alpha_dux* ) \
		MKAFS_OSTYPE=DUX;; \
	sgi_6* ) \
		MKAFS_OSTYPE=IRIX;; \
	ppc_darwin* ) \
		MKAFS_OSTYPE=DARWIN;; \
	* ) \
		echo WARNING: No MakefileProto for sys ${SYS_NAME} ; \
	esac; \
	obj/config/config src/libuafs/MakefileProto.$$MKAFS_OSTYPE obj/libuafs/Makefile ${SYS_NAME}

# "COMPILE_PART1=${COMPILE_PART1}" "COMPILE_PART2=${COMPILE_PART2}" SYS_NAME=${SYS_NAME} ;; 

TARGET=finale
install:
	$(MAKE) $(TARGET) "SYS_NAME=${SYS_NAME}" "WASHTOOL=${WASHTOOL}" \
		"COMPILE_PART1=${COMPILE_PART1}" \
		"COMPILE_PART2=${COMPILE_PART2}" \
		 DESTDIR=`pwd`/${SYS_NAME}/dest/ \
		 SRCDIR=`pwd`/${SYS_NAME}/dest/ \
		 "DBG_DEFS=${DBG_DEFS}"

# sgi: "NON_SHARED=-non_shared"
#updatekdir:
#	${WASHTOOL} rcsupdate ${KERNELDIR} libafs

kupdate: updatekdir libafs_setup libuafs_setup

blibafs:
	$(MAKE) libafs "WASHTOOL=${WASHTOOL}" "WASHTOOLFLAGS=-argument kinstall" SYS_NAME=${SYS_NAME} "CPUARCH=${CPUARCH}" "MPSP=${MPSP}" "DBG_DEFS=${DBG_DEFS}"

blibuafs:
	$(MAKE) libuafs "WASHTOOL=${WASHTOOL}" "WASHTOOLFLAGS=-argument ukinstall" SYS_NAME=${SYS_NAME} "CPUARCH=${CPUARCH}" "MPSP=${MPSP}" "DBG_DEFS=${DBG_DEFS}"

bwebafs: sys
	$(MAKE) libuafs "WASHTOOL=${WASHTOOL}" "WASHTOOLFLAGS=-argument webinstall" SYS_NAME=${SYS_NAME} "CPUARCH=${CPUARCH}" "MPSP=${MPSP}" "DBG_DEFS=${DBG_DEFS}"

clean:
	${WASHTOOL} every "cd obj/%s; $(MAKE) clean" -rootd obj

checklist:
	${WASHTOOL} every -command \
	  'cd rcs/%s; echo ; echo In %s:; if test x = "x`/bin/ls RCS/*,v`"; then exit 0; else rlog -I RCS/*,v; fi'

links:
	echo "Remove the obj directory"
	- /bin/rm -rf @sys/obj/* ;
	- /bin/rm -rf @sys/dest/* ;
	- /bin/mkdir @sys/obj ;
	echo "Create the obj directories"
	here=`pwd` ; \
	cd src; \
	for file in `find [!NW]* -type d -print` ; do \
		/bin/mkdir $${here}/obj/$${file} ; \
	done;
	echo "Create the obj file links"
	here=`pwd` ; \
	cd src; \
	for file in `find [!NW]* -type f -print` ; do \
		/bin/ln -s `pwd`/$${file} $${here}/obj/$${file} ; \
	done;

#
# Below targets are all deprecated, insecure, or obsolte, 
# see README.OBSOLETE and README.INSECURE for more info
#

mpp: project
	@case ${WITH_DEPRECATED} in \
	YES) 	${COMPILE_PART1} mpp ${COMPILE_PART2} ;; \
	*)	echo skipping deprecated target: mpp ;; \
	esac

package: project mpp
	@case ${WITH_DEPRECATED} in \
	YES) 	${COMPILE_PART1} package ${COMPILE_PART2} ;; \
	*)	echo skipping deprecated target: package ;; \
	esac

package.files: package
	@case ${WITH_DEPRECATED} in \
	YES) 	${DESTDIR}/bin/makepkgfiles afs.dev ;; \
	*)	echo skipping deprecated target: package.files ;; \
	esac

ntp: project volser
	@case ${WITH_DEPRECATED} in \
	YES) 	case ${SYS_NAME} in \
		sun4x_58 | *linux* | *fbsd* | ppc_darwin* ) echo skipping ntp for ${SYS_NAME} ;; \
		* ) ${COMPILE_PART1} ntp ${COMPILE_PART2} ;; \
		esac ;; \
	*)	echo skipping deprecated target: ntp ;; \
	esac

sgistuff: project rlogind
	@case ${WITH_DEPRECATED} in \
	YES) 	case ${SYS_NAME} in \
		sgi_* )	${COMPILE_PART1} sgistuff ${COMPILE_PART2} ;; \
		* ) echo skipping sgistuff for ${SYS_NAME} ;; \
		esac ;; \
	*)	echo skipping deprecated target: sgistuff ;; \
	esac

ftpd43+: project kauth rxkad
	@case ${WITH_DEPRECATED} in \
	YES) 	case ${SYS_NAME} in \
		rs_aix* | sun4x_55 | *linux*) \
			${COMPILE_PART1} ftpd43+ ${COMPILE_PART2} ;; \
		*) echo skip ftpd43+ for ${SYS_NAME} ;; \
		esac ;; \
	*)	echo skipping deprecated target: ftpd43+ ;; \
	esac

inetd: project kauth rxkad
	@case ${WITH_DEPRECATED} in \
	YES) 	${COMPILE_PART1} inetd ${COMPILE_PART2} ;; \
	*)	echo skipping deprecated target: inetd ;; \
	esac

rsh: project inetd
	@case ${WITH_DEPRECATED} in \
	YES) 	${COMPILE_PART1} rsh ${COMPILE_PART2} ;; \
	*)	echo skipping deprecated target: rsh ;; \
	esac

rlogind: project rsh ftpd43+ login
	@case ${WITH_DEPRECATED} in \
	YES) 	case ${SYS_NAME} in \
		rs_aix*) ${COMPILE_PART1} rlogind ${COMPILE_PART2} ;; \
		*) echo skipping rlogind for ${SYS_NAME} ;; \
		esac ;; \
	*)	echo skipping deprecated target: rlogind ;; \
	esac

rcp: project rsh inetd
	@case ${WITH_DEPRECATED} in \
	YES) 	case ${SYS_NAME} in \
		sgi_* )	echo skip rcp for ${SYS_NAME} ;; \
		*)	${COMPILE_PART1} rcp ${COMPILE_PART2} ;; \
		esac ;; \
	*)	echo skipping deprecated target: rcp ;; \
	esac

allrcmds: project rcp rlogind


