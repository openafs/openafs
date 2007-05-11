# Copyright 2006-2007, Sine Nomine Associates and others.
# All Rights Reserved.
# 
# This software has been released under the terms of the IBM Public
# License.  For details, see the LICENSE file in the top-level source
# directory or online at http://www.openafs.org/dl/license10.html


AUDIT_SRCDIR=$(TOP_SRCDIR)/audit
AUTH_SRCDIR=$(TOP_SRCDIR)/auth
KAUTH_SRCDIR=$(TOP_SRCDIR)/kauth
RXKAD_SRCDIR=$(TOP_SRCDIR)/rxkad
PTSERVER_SRCDIR=$(TOP_SRCDIR)/ptserver
SYS_SRCDIR=$(TOP_SRCDIR)/sys
UBIK_SRCDIR=$(TOP_SRCDIR)/ubik
UTIL_SRCDIR=$(TOP_SRCDIR)/util

AUDIT_OBJDIR=$(TOP_OBJDIR)/src/audit
AUTH_OBJDIR=$(TOP_OBJDIR)/src/auth
KAUTH_OBJDIR=$(TOP_OBJDIR)/src/kauth
RXKAD_OBJDIR=$(TOP_OBJDIR)/src/rxkad
PTSERVER_OBJDIR=$(TOP_OBJDIR)/src/ptserver
SYS_OBJDIR=$(TOP_OBJDIR)/src/sys
UBIK_OBJDIR=$(TOP_OBJDIR)/src/ubik
UTIL_OBJDIR=$(TOP_OBJDIR)/src/util


LIBAFSAUTHENT_VERSION_MAJOR = 1
LIBAFSAUTHENT_VERSION_MINOR = 0
LIBAFSAUTHENT_DEPS_libafsauthent_shlib_pthread_inst_32 = \
	${MT_LIBS} ${OSI_LIBS} \
	-L$(TOP_OBJDIR)/lib -losi-pthread-inst-32 -lafsrpc-pthread-inst-32 \
	$(NULL)
LIBAFSAUTHENT_DEPS_libafsauthent_shlib_pthread_inst_64 = \
	${MT_LIBS} ${OSI_LIBS} \
	-L$(TOP_OBJDIR)/lib -losi-pthread-inst-64 -lafsrpc-pthread-inst-64 \
	$(NULL)
LIBAFSAUTHENT_DEPS_libafsauthent_shlib_pthread_ni_32 = \
	${MT_LIBS} ${OSI_LIBS} \
	-L$(TOP_OBJDIR)/lib -losi-pthread-ni-32 -lafsrpc-pthread-ni-32 \
	$(NULL)
LIBAFSAUTHENT_DEPS_libafsauthent_shlib_pthread_ni_64 = \
	${MT_LIBS} ${OSI_LIBS} \
	-L$(TOP_OBJDIR)/lib -losi-pthread-ni-64 -lafsrpc-pthread-ni-64 \
	$(NULL)

# core libafsauthent build objects
# for all builds, kernel, ukernel, and userspace

LIBAFSAUTHENT_CFLAGS_COMMON = 
LIBAFSAUTHENT_OBJS_COMMON = \
	$(NULL)

LIBAFSAUTHENT_CFLAGS_kernelspace = 
LIBAFSAUTHENT_OBJS_kernelspace = \
	$(NULL)

LIBAFSAUTHENT_CFLAGS_kernel = 
LIBAFSAUTHENT_OBJS_kernel = \
	$(NULL)

LIBAFSAUTHENT_CFLAGS_userspace = 
LIBAFSAUTHENT_OBJS_userspace = \
	$(NULL)

LIBAFSAUTHENT_CFLAGS_ukernel = 
LIBAFSAUTHENT_OBJS_ukernel = \
	$(NULL)

LIBAFSAUTHENT_CFLAGS_libafsauthent = \
	${COMMON_INCLUDE} ${CFLAGS} ${OSI_CFLAGS} ${ARCHFLAGS} ${XCFLAGS} \
	-DRXDEBUG -I$(DES_SRCDIR) -I$(DES_OBJDIR) -I$(RXKAD_SRCDIR) \
	-I$(RXKAD_OBJDIR) -I$(FSINT_SRCDIR) -I$(FSINT_OBJDIR) \
	$(NULL)
LIBAFSAUTHENT_OBJS_libafsauthent = \
	audit.o \
	auth_acfg_errors.o \
	auth_authcon.o \
	auth_cellconfig.o \
	auth_ktc.o \
	auth_ktc_errors.o \
	auth_userok.o \
	auth_writeconfig.o \
	kauth_authclient.o \
	kauth_client.o \
	kaaux.o \
	kaerrors.o \
	kalocalcell.o \
	kauth.cs.o \
	kauth.xdr.o \
	kautils.o \
	kauth_read_passwd.o \
	kauth_token.o \
	kauth_user.o \
	ubik_init.o \
	ubikclient.o \
	ubik_errors.o \
	ubik_int.cs.o \
	ubik_int.xdr.o \
	ubik_client_tracepoint.o \
	util_get_krbrlm.o \
	util_dirpath.o \
	util_serverLog.o \
	util_snprintf.o \
	util_fileutil.o \
	rxkad_errs.o \
	sys_rmtsysc.o \
	sys_rmtsys.xdr.o \
	sys_rmtsys.cs.o \
	sys_afssyscalls.o \
	sys_rmtsysnet.o \
	sys_glue.o \
	sys_setpag.o \
	sys_pioctl.o \
	ptclient.o \
	ptint.cs.o \
	ptint.xdr.o \
	ptuser.o \
	ptserver_display.o \
	pterror.o \
	$(NULL)


# userspace threading model-specific build objects

LIBAFSAUTHENT_CFLAGS_userspace_lwp = 
LIBAFSAUTHENT_OBJS_userspace_lwp = \
	$(NULL)

LIBAFSAUTHENT_CFLAGS_ukernel_lwp = 
LIBAFSAUTHENT_OBJS_ukernel_lwp = \
	$(NULL)

LIBAFSAUTHENT_CFLAGS_libafsauthent_lwp = 
LIBAFSAUTHENT_OBJS_libafsauthent_lwp = \
	$(NULL)

LIBAFSAUTHENT_CFLAGS_userspace_pthread = ${MT_CFLAGS}
LIBAFSAUTHENT_OBJS_userspace_pthread = \
	$(NULL)

LIBAFSAUTHENT_CFLAGS_ukernel_pthread = 
LIBAFSAUTHENT_OBJS_ukernel_pthread = \
	$(NULL)

LIBAFSAUTHENT_CFLAGS_libafsauthent_pthread = 
LIBAFSAUTHENT_OBJS_libafsauthent_pthread = \
	util_pthread_glock.o \
	$(NULL)


# osi trace build objects for
# all build environments, kernel, ukernel, and userspace

LIBAFSAUTHENT_CFLAGS_COMMON_inst =
LIBAFSAUTHENT_OBJS_COMMON_inst = \
	$(NULL)

LIBAFSAUTHENT_CFLAGS_kernelspace_inst = 
LIBAFSAUTHENT_OBJS_kernelspace_inst = \
	$(NULL)

LIBAFSAUTHENT_CFLAGS_kernel_inst = 
LIBAFSAUTHENT_OBJS_kernel_inst = \
	$(NULL)

LIBAFSAUTHENT_CFLAGS_userspace_inst = 
LIBAFSAUTHENT_OBJS_userspace_inst = \
	$(NULL)

# totally disable tracing on UKERNEL for now
# due to ukernel's complete brokenness
# (e.g. no threading model is specified)
LIBAFSAUTHENT_CFLAGS_ukernel_inst = -DOSI_TRACE_DISABLE
LIBAFSAUTHENT_OBJS_ukernel_inst = \
	$(NULL)

LIBAFSAUTHENT_CFLAGS_libafsauthent_inst = 
LIBAFSAUTHENT_OBJS_libafsauthent_inst = \
	$(NULL)

LIBAFSAUTHENT_CFLAGS_COMMON_ni = -DOSI_TRACE_DISABLE
LIBAFSAUTHENT_OBJS_COMMON_ni = \
	$(NULL)

LIBAFSAUTHENT_CFLAGS_kernelspace_ni = 
LIBAFSAUTHENT_OBJS_kernelspace_ni = \
	$(NULL)

LIBAFSAUTHENT_CFLAGS_kernel_ni = 
LIBAFSAUTHENT_OBJS_kernel_ni = \
	$(NULL)

LIBAFSAUTHENT_CFLAGS_userspace_ni = 
LIBAFSAUTHENT_OBJS_userspace_ni = \
	$(NULL)

LIBAFSAUTHENT_CFLAGS_ukernel_ni =
LIBAFSAUTHENT_OBJS_ukernel_ni = \
	$(NULL)

LIBAFSAUTHENT_CFLAGS_libafsauthent_ni = 
LIBAFSAUTHENT_OBJS_libafsauthent_ni = \
	$(NULL)


LIBAFSAUTHENT_CFLAGS_COMMON_32 = 
LIBAFSAUTHENT_OBJS_COMMON_32 = \
	$(NULL)

LIBAFSAUTHENT_CFLAGS_kernelspace_32 = 
LIBAFSAUTHENT_OBJS_kernelspace_32 = \
	$(NULL)

LIBAFSAUTHENT_CFLAGS_kernel_32 = 
LIBAFSAUTHENT_OBJS_kernel_32 = \
	$(NULL)

LIBAFSAUTHENT_CFLAGS_userspace_32 = 
LIBAFSAUTHENT_OBJS_userspace_32 = \
	$(NULL)

LIBAFSAUTHENT_CFLAGS_ukernel_32 = 
LIBAFSAUTHENT_OBJS_ukernel_32 = \
	$(NULL)

LIBAFSAUTHENT_CFLAGS_libafsauthent_32 = 
LIBAFSAUTHENT_OBJS_libafsauthent_32 = \
	$(NULL)


LIBAFSAUTHENT_CFLAGS_COMMON_64 = 
LIBAFSAUTHENT_OBJS_COMMON_64 = \
	$(NULL)

LIBAFSAUTHENT_CFLAGS_kernelspace_64 = 
LIBAFSAUTHENT_OBJS_kernelspace_64 = \
	$(NULL)

LIBAFSAUTHENT_CFLAGS_kernel_64 = 
LIBAFSAUTHENT_OBJS_kernel_64 = \
	$(NULL)

LIBAFSAUTHENT_CFLAGS_userspace_64 = 
LIBAFSAUTHENT_OBJS_userspace_64 = \
	$(NULL)

LIBAFSAUTHENT_CFLAGS_ukernel_64 = 
LIBAFSAUTHENT_OBJS_ukernel_64 = \
	$(NULL)

LIBAFSAUTHENT_CFLAGS_libafsauthent_64 = 
LIBAFSAUTHENT_OBJS_libafsauthent_64 = \
	$(NULL)

# archive versus shlib stuff

LIBAFSAUTHENT_CFLAGS_libafsauthent_archive =
LIBAFSAUTHENT_OBJS_libafsauthent_archive = \
	$(NULL)

LIBAFSAUTHENT_CFLAGS_libafsauthent_shlib = ${SHLIB_CFLAGS}
LIBAFSAUTHENT_OBJS_libafsauthent_shlib = \
	$(NULL)


# build rules for OS Interface library

audit.o: $(AUDIT_SRCDIR)/audit.c
	$(CRULE_OPT)

auth_cellconfig.o: $(AUTH_SRCDIR)/cellconfig.c
	$(CRULE_OPT)

auth_ktc.o: $(AUTH_SRCDIR)/ktc.c
	$(CRULE_OPT)

auth_userok.o: $(AUTH_SRCDIR)/userok.c
	$(CRULE_OPT)

auth_writeconfig.o: $(AUTH_SRCDIR)/writeconfig.c
	$(CRULE_OPT)

auth_authcon.o: $(AUTH_SRCDIR)/authcon.c
	$(CRULE_OPT)

auth_ktc_errors.o: $(AUTH_OBJDIR)/ktc_errors.c
	$(CRULE_OPT)

auth_acfg_errors.o: $(AUTH_OBJDIR)/acfg_errors.c
	$(CRULE_OPT)

kauth.xdr.o: $(KAUTH_OBJDIR)/kauth.xdr.c
	$(CRULE_OPT)

kauth.cs.o: $(KAUTH_OBJDIR)/kauth.cs.c
	$(CRULE_OPT)

kaaux.o: $(KAUTH_SRCDIR)/kaaux.c
	$(CRULE_OPT)

kauth_client.o: $(KAUTH_SRCDIR)/client.c
	$(CRULE_OPT)

kauth_authclient.o: $(KAUTH_SRCDIR)/authclient.c
	$(CRULE_OPT)

kauth_token.o: $(KAUTH_SRCDIR)/token.c
	$(CRULE_OPT)

kautils.o: $(KAUTH_SRCDIR)/kautils.c
	$(CRULE_OPT)

kalocalcell.o: $(KAUTH_SRCDIR)/kalocalcell.c
	$(CRULE_OPT)

kaerrors.o: $(KAUTH_OBJDIR)/kaerrors.c
	$(CRULE_OPT)

kauth_user.o: $(KAUTH_SRCDIR)/user.c
	$(CRULE_OPT)

kauth_read_passwd.o: $(KAUTH_SRCDIR)/read_passwd.c
	$(CRULE_OPT)

ubikclient.o: $(UBIK_SRCDIR)/ubikclient.c
	$(CRULE_OPT)

ubik_init.o: $(UBIK_SRCDIR)/uinit.c
	$(CRULE_OPT)

ubik_errors.o: $(UBIK_OBJDIR)/uerrors.c
	$(CRULE_OPT)

ubik_int.cs.o: $(UBIK_OBJDIR)/ubik_int.cs.c
	$(CRULE_OPT)

ubik_int.xdr.o: $(UBIK_OBJDIR)/ubik_int.xdr.c
	$(CRULE_OPT)

ubik_client_tracepoint.o: $(UBIK_OBJDIR)/ubik_client_tracepoint.c
	$(CRULE_OPT)

util_get_krbrlm.o: $(UTIL_SRCDIR)/get_krbrlm.c
	$(CRULE_OPT)

util_dirpath.o: $(UTIL_SRCDIR)/dirpath.c
	$(CRULE_OPT)

util_serverLog.o: $(UTIL_SRCDIR)/serverLog.c
	$(CRULE_OPT)

util_snprintf.o: $(UTIL_SRCDIR)/snprintf.c
	$(CRULE_OPT)

util_fileutil.o: $(UTIL_SRCDIR)/fileutil.c
	$(CRULE_OPT)

util_pthread_glock.o: $(UTIL_SRCDIR)/pthread_glock.c
	$(CRULE_OPT)

rxkad_errs.o: $(RXKAD_OBJDIR)/rxkad_errs.c
	$(CRULE_OPT)

ptclient.o: $(PTSERVER_SRCDIR)/ptclient.c
	$(CRULE_OPT)

# The special treatment of this file for hp_ux110 is because of a bug
# in version A.11.01.00 of the HP C compiler.  This bug appears to be
# fixed in version A.11.01.02 of the HP C compiler, however this version
# of the compiler is not installed on all of our build machines.
# The symptom of the problem is an error when linking the pthread fileserver:
# /usr/ccs/bin/ld: TP override with DATA_ONE_SYM fixup for non thread local
# storage symbol pr_Initialize in file DEST/lib/libafsauthent.a(ptuser.o)
ptuser.o: $(PTSERVER_SRCDIR)/ptuser.c
	set -x; \
	case ${SYS_NAME} in \
	hp_ux11*) \
		set X `echo $(CRULE_OPT) | sed s/-g//`; shift; \
		"$$@" \
		;; \
	*) \
		$(CRULE_OPT) \
		;; \
	esac

ptserver_display.o: $(PTSERVER_SRCDIR)/display.c
	$(CRULE_OPT)

ptint.cs.o: $(PTSERVER_OBJDIR)/ptint.cs.c
	$(CRULE_OPT)

ptint.xdr.o: $(PTSERVER_OBJDIR)/ptint.xdr.c
	$(CRULE_OPT)

pterror.o: $(PTSERVER_OBJDIR)/pterror.c
	$(CRULE_OPT)

sys_rmtsysc.o: $(SYS_SRCDIR)/rmtsysc.c
	$(CRULE_OPT)

sys_rmtsys.xdr.o: $(SYS_OBJDIR)/rmtsys.xdr.c
	$(CRULE_OPT)

sys_rmtsys.cs.o: $(SYS_OBJDIR)/rmtsys.cs.c
	$(CRULE_OPT)

sys_afssyscalls.o: $(SYS_SRCDIR)/afssyscalls.c
	$(CRULE_OPT)

sys_rmtsysnet.o: $(SYS_SRCDIR)/rmtsysnet.c
	$(CRULE_OPT)

sys_glue.o: $(SYS_SRCDIR)/glue.c
	$(CRULE_OPT)

sys_setpag.o: $(SYS_SRCDIR)/setpag.c
	$(CRULE_OPT)

sys_pioctl.o: $(SYS_SRCDIR)/pioctl.c
	$(CRULE_OPT)
