# Copyright 2006-2007, Sine Nomine Associates and others.
# All Rights Reserved.
# 
# This software has been released under the terms of the IBM Public
# License.  For details, see the LICENSE file in the top-level source
# directory or online at http://www.openafs.org/dl/license10.html


COMERR_SRCDIR=$(TOP_SRCDIR)/comerr
DES_SRCDIR=$(TOP_SRCDIR)/des
FSINT_SRCDIR=$(TOP_SRCDIR)/fsint
LWP_SRCDIR=$(TOP_SRCDIR)/lwp
OSI_SRCDIR=$(TOP_SRCDIR)/osi
RX_SRCDIR=$(TOP_SRCDIR)/rx
RXKAD_SRCDIR=$(TOP_SRCDIR)/rxkad
RXSTAT_SRCDIR=$(TOP_SRCDIR)/rxstat
SYS_SRCDIR=$(TOP_SRCDIR)/sys
TRACE_SRCDIR=$(TOP_SRCDIR)/trace
UTIL_SRCDIR=$(TOP_SRCDIR)/util

COMERR_OBJDIR=$(TOP_OBJDIR)/src/comerr
DES_OBJDIR=$(TOP_OBJDIR)/src/des
FSINT_OBJDIR=$(TOP_OBJDIR)/src/fsint
LWP_OBJDIR=$(TOP_OBJDIR)/src/lwp
OSI_OBJDIR=$(TOP_OBJDIR)/src/osi
RX_OBJDIR=$(TOP_OBJDIR)/src/rx
RXKAD_OBJDIR=$(TOP_OBJDIR)/src/rxkad
RXSTAT_OBJDIR=$(TOP_OBJDIR)/src/rxstat
SYS_OBJDIR=$(TOP_OBJDIR)/src/sys
TRACE_OBJDIR=$(TOP_OBJDIR)/src/trace
UTIL_OBJDIR=$(TOP_OBJDIR)/src/util


LIBAFSRPC_VERSION_MAJOR = 1
LIBAFSRPC_VERSION_MINOR = 1
LIBAFSRPC_DEPS_libafsrpc_shlib_pthread_inst = ${MT_LIBS} ${OSI_LIBS} -L$(TOP_OBJDIR)/lib -losi-pthread-inst
LIBAFSRPC_DEPS_libafsrpc_shlib_pthread_ni = ${MT_LIBS} ${OSI_LIBS} -L$(TOP_OBJDIR)/lib -losi-pthread-ni

# core libafsrpc build objects
# for all builds, kernel, ukernel, and userspace

LIBAFSRPC_CFLAGS_COMMON = 
LIBAFSRPC_OBJS_COMMON = \
	$(NULL)

LIBAFSRPC_CFLAGS_kernelspace = 
LIBAFSRPC_OBJS_kernelspace = \
	$(NULL)

LIBAFSRPC_CFLAGS_kernel = 
LIBAFSRPC_OBJS_kernel = \
	$(NULL)

LIBAFSRPC_CFLAGS_userspace = 
LIBAFSRPC_OBJS_userspace = \
	$(NULL)

LIBAFSRPC_CFLAGS_ukernel = 
LIBAFSRPC_OBJS_ukernel = \
	$(NULL)

LIBAFSRPC_CFLAGS_libafsrpc = \
	${COMMON_INCLUDE} ${CFLAGS} ${OSI_CFLAGS} ${ARCHFLAGS} ${XCFLAGS} \
	-DRXDEBUG -I$(DES_SRCDIR) -I$(DES_OBJDIR) -I$(RXKAD_SRCDIR) \
	-I$(RXKAD_OBJDIR) -I$(FSINT_SRCDIR) -I$(FSINT_OBJDIR) \
	$(NULL)
LIBAFSRPC_OBJS_libafsrpc = \
	AFS_component_version_number.o \
	afsint.cs.o \
	afsint.xdr.o \
	afscbint.cs.o \
	afscbint.xdr.o \
	afsaux.o \
	comerr_error_msg.o \
	comerr_et_name.o \
	comerr_com_err.o \
	des.o \
	des_cbc_encrypt.o \
	des_cksum.o \
	des_crypt.o \
	des_debug_decl.o \
	des_key_parity.o \
	des_key_sched.o \
	des_misc.o \
	des_new_rnd_key.o \
	des_pcbc_encrypt.o \
	des_quad_cksum.o \
	des_strng_to_key.o \
	des_util.o \
	des_weak_key.o \
	lwp_fasttime.o \
	rx.o \
	rx_conncache.o \
	rx_event.o \
	rx_getaddr.o \
	rx_globals.o \
	rx_misc.o \
	rx_multi.o \
	rx_null.o \
	rx_packet.o \
	rx_rdwr.o \
	rx_trace.o \
	rx_user.o \
	rxkad_client.o \
	rxkad_server.o \
	rxkad_common.o \
	rxkad_ticket.o \
	rxkad_ticket5.o \
	rxkad_crc.o \
	rxkad_md4.o \
	rxkad_md5.o \
	rxkad_fcrypt.o \
	rxkad_crypt_conn.o \
	rxstat.o \
	rxstat.ss.o \
	rxstat.xdr.o \
	rxstat.cs.o \
	sys_syscall.o \
	util_assert.o \
	util_casestrcpy.o \
	util_base64.o \
	xdr.o \
	xdr_array.o \
	xdr_arrayn.o \
	xdr_rx.o \
	xdr_int32.o \
	xdr_int64.o \
	xdr_afsuuid.o \
	xdr_update.o \
	xdr_reference.o \
	$(NULL)


# userspace threading model-specific build objects

LIBAFSRPC_CFLAGS_userspace_lwp = 
LIBAFSRPC_OBJS_userspace_lwp = \
	$(NULL)

LIBAFSRPC_CFLAGS_ukernel_lwp = 
LIBAFSRPC_OBJS_ukernel_lwp = \
	$(NULL)

LIBAFSRPC_CFLAGS_libafsrpc_lwp = 
LIBAFSRPC_OBJS_libafsrpc_lwp = \
	rx_lwp.o \
	lwp_iomgr.o \
	lwp.o \
	lwp_preempt.o \
	lwp_process.o \
	lwp_threadname.o \
	lwp_timer.o \
	lwp_waitkey.o \
	$(NULL)

LIBAFSRPC_CFLAGS_userspace_pthread = ${MT_CFLAGS}
LIBAFSRPC_OBJS_userspace_pthread = \
	$(NULL)

LIBAFSRPC_CFLAGS_ukernel_pthread = 
LIBAFSRPC_OBJS_ukernel_pthread = \
	$(NULL)

LIBAFSRPC_CFLAGS_libafsrpc_pthread = 
LIBAFSRPC_OBJS_libafsrpc_pthread = \
	rx_pthread.o \
	$(NULL)


# osi trace build objects for
# all build environments, kernel, ukernel, and userspace

LIBAFSRPC_CFLAGS_COMMON_inst =
LIBAFSRPC_OBJS_COMMON_inst = \
	$(NULL)

LIBAFSRPC_CFLAGS_kernelspace_inst = 
LIBAFSRPC_OBJS_kernelspace_inst = \
	$(NULL)

LIBAFSRPC_CFLAGS_kernel_inst = 
LIBAFSRPC_OBJS_kernel_inst = \
	$(NULL)

LIBAFSRPC_CFLAGS_userspace_inst = 
LIBAFSRPC_OBJS_userspace_inst = \
	$(NULL)

# totally disable tracing on UKERNEL for now
# due to ukernel's complete brokenness
# (e.g. no threading model is specified)
LIBAFSRPC_CFLAGS_ukernel_inst = -DOSI_TRACE_DISABLE
LIBAFSRPC_OBJS_ukernel_inst = \
	$(NULL)

LIBAFSRPC_CFLAGS_libafsrpc_inst = 
LIBAFSRPC_OBJS_libafsrpc_inst = \
	rx_tracepoint_table.o \
	$(NULL)

LIBAFSRPC_CFLAGS_COMMON_ni = -DOSI_TRACE_DISABLE
LIBAFSRPC_OBJS_COMMON_ni = \
	$(NULL)

LIBAFSRPC_CFLAGS_kernelspace_ni = 
LIBAFSRPC_OBJS_kernelspace_ni = \
	$(NULL)

LIBAFSRPC_CFLAGS_kernel_ni = 
LIBAFSRPC_OBJS_kernel_ni = \
	$(NULL)

LIBAFSRPC_CFLAGS_userspace_ni = 
LIBAFSRPC_OBJS_userspace_ni = \
	$(NULL)

LIBAFSRPC_CFLAGS_ukernel_ni =
LIBAFSRPC_OBJS_ukernel_ni = \
	$(NULL)

LIBAFSRPC_CFLAGS_libafsrpc_ni = 
LIBAFSRPC_OBJS_libafsrpc_ni = \
	$(NULL)


LIBAFSRPC_CFLAGS_COMMON_32 = 
LIBAFSRPC_OBJS_COMMON_32 = \
	$(NULL)

LIBAFSRPC_CFLAGS_kernelspace_32 = 
LIBAFSRPC_OBJS_kernelspace_32 = \
	$(NULL)

LIBAFSRPC_CFLAGS_kernel_32 = 
LIBAFSRPC_OBJS_kernel_32 = \
	$(NULL)

LIBAFSRPC_CFLAGS_userspace_32 = 
LIBAFSRPC_OBJS_userspace_32 = \
	$(NULL)

LIBAFSRPC_CFLAGS_ukernel_32 = 
LIBAFSRPC_OBJS_ukernel_32 = \
	$(NULL)

LIBAFSRPC_CFLAGS_libafsrpc_32 = 
LIBAFSRPC_OBJS_libafsrpc_32 = \
	$(NULL)


LIBAFSRPC_CFLAGS_COMMON_64 = 
LIBAFSRPC_OBJS_COMMON_64 = \
	$(NULL)

LIBAFSRPC_CFLAGS_kernelspace_64 = 
LIBAFSRPC_OBJS_kernelspace_64 = \
	$(NULL)

LIBAFSRPC_CFLAGS_kernel_64 = 
LIBAFSRPC_OBJS_kernel_64 = \
	$(NULL)

LIBAFSRPC_CFLAGS_userspace_64 = 
LIBAFSRPC_OBJS_userspace_64 = \
	$(NULL)

LIBAFSRPC_CFLAGS_ukernel_64 = 
LIBAFSRPC_OBJS_ukernel_64 = \
	$(NULL)

LIBAFSRPC_CFLAGS_libafsrpc_64 = 
LIBAFSRPC_OBJS_libafsrpc_64 = \
	$(NULL)

# archive versus shlib stuff

LIBAFSRPC_CFLAGS_libafsrpc_archive =
LIBAFSRPC_OBJS_libafsrpc_archive = \
	$(NULL)

LIBAFSRPC_CFLAGS_libafsrpc_shlib = ${SHLIB_CFLAGS}
LIBAFSRPC_OBJS_libafsrpc_shlib = \
	$(NULL)


# build rules for OS Interface library

rx_event.o: $(RX_SRCDIR)/rx_event.c
	$(CRULE_OPT)

rx_user.o: $(RX_SRCDIR)/rx_user.c
	$(CRULE_OPT)

rx_lwp.o: $(RX_SRCDIR)/rx_lwp.c
	$(CRULE_OPT)

rx_pthread.o: $(RX_SRCDIR)/rx_pthread.c
	$(CRULE_OPT)

rx.o: $(RX_SRCDIR)/rx.c
	$(CRULE_OPT)

rx_conncache.o: $(RX_SRCDIR)/rx_conncache.c
	$(CRULE_OPT)

rx_null.o: $(RX_SRCDIR)/rx_null.c
	$(CRULE_OPT)

rx_globals.o: $(RX_SRCDIR)/rx_globals.c 
	$(CRULE_OPT)

rx_getaddr.o: $(RX_SRCDIR)/rx_getaddr.c
	$(CRULE_OPT)

rx_misc.o: $(RX_SRCDIR)/rx_misc.c
	$(CRULE_OPT)

rx_packet.o: $(RX_SRCDIR)/rx_packet.c
	$(CRULE_OPT)

rx_rdwr.o: $(RX_SRCDIR)/rx_rdwr.c
	$(CRULE_OPT)

rx_trace.o: $(RX_SRCDIR)/rx_trace.c
	$(CRULE_OPT)

rx_multi.o: $(RX_SRCDIR)/rx_multi.c
	$(CRULE_OPT)

rx_tracepoint_table.o: $(RX_OBJDIR)/tracepoint_table.c
	$(CRULE_OPT)

rxkad_client.o: $(RXKAD_SRCDIR)/rxkad_client.c
	$(CRULE_OPT)

rxkad_server.o: $(RXKAD_SRCDIR)/rxkad_server.c
	$(CRULE_OPT)

rxkad_common.o: $(RXKAD_SRCDIR)/rxkad_common.c
	$(CRULE_OPT)

rxkad_ticket.o: $(RXKAD_SRCDIR)/ticket.c
	$(CRULE_OPT)

rxkad_ticket5.o: $(RXKAD_SRCDIR)/ticket5.c
	$(CRULE_OPT)

rxkad_crc.o: $(RXKAD_SRCDIR)/crc.c
	$(CRULE_OPT)

rxkad_md4.o: $(RXKAD_SRCDIR)/md4.c
	$(CRULE_OPT)

rxkad_md5.o: $(RXKAD_SRCDIR)/md5.c
	$(CRULE_OPT)

rxkad_fcrypt.o: $(RXKAD_SRCDIR)/domestic/fcrypt.c
	$(CRULE_OPT)

rxkad_crypt_conn.o: $(RXKAD_SRCDIR)/domestic/crypt_conn.c
	$(CRULE_OPT)

AFS_component_version_number.o: $(RX_OBJDIR)/AFS_component_version_number.c
	$(CRULE_OPT)

xdr.o: $(RX_SRCDIR)/xdr.c
	$(CRULE_OPT)

xdr_int32.o: $(RX_SRCDIR)/xdr_int32.c
	$(CRULE_OPT)

xdr_int64.o: $(RX_SRCDIR)/xdr_int64.c
	$(CRULE_OPT)

xdr_array.o: $(RX_SRCDIR)/xdr_array.c
	$(CRULE_OPT)

xdr_arrayn.o: $(RX_SRCDIR)/xdr_arrayn.c
	$(CRULE_OPT)

xdr_float.o: $(RX_SRCDIR)/xdr_float.c
	$(CRULE_OPT)

xdr_mem.o: $(RX_SRCDIR)/xdr_mem.c
	$(CRULE_OPT)

xdr_rec.o: $(RX_SRCDIR)/xdr_rec.c
	$(CRULE_OPT)

xdr_reference.o: $(RX_SRCDIR)/xdr_refernce.c
	$(CRULE_OPT)

xdr_rx.o: $(RX_SRCDIR)/xdr_rx.c
	$(CRULE_OPT)

xdr_update.o: $(RX_SRCDIR)/xdr_update.c
	$(CRULE_OPT)

xdr_afsuuid.o: $(RX_SRCDIR)/xdr_afsuuid.c
	$(CRULE_OPT)

# Note that the special case statement for compiling des.c is present
# simply to work around a compiler bug on HP-UX 11.0.  The symptom of
# the problem is that linking the pthread fileserver fails with messages
# such as
#
#   pxdb internal warning: cu[84]: SLT_SRCFILE[411] out of synch
#   Please contact your HP Support representative
#   pxdb internal warning: cu[84]: SLT_SRCFILE[442] out of synch
#   pxdb internal warning: cu[84]: SLT_SRCFILE[450] out of synch
#   pxdb internal warning: cu[84]: SLT_SRCFILE[529] out of synch
#   pxdb internal warning: cu[84]: SLT_SRCFILE[544] out of synch
#   ...
#   pxdb32: internal error. File won't be debuggable (still a valid executable)
#   *** Error exit code 10
#
# The problematic version of pxdb is:
#
#   $ what /opt/langtools/bin/pxdb32
#   /opt/langtools/bin/pxdb32:
#           HP92453-02 A.10.0A HP-UX SYMBOLIC DEBUGGER (PXDB) $Revision$
#
# The problem occurs when -g and -O are both used when compiling des.c.
# The simplest way to work around the problem is to leave out either -g or -O.
# Since des.c is relatively stable I've chosen to eliminate -g rather
# than take any hit in performance.

des.o: $(DES_SRCDIR)/des.c
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

des_crypt.o: $(DES_SRCDIR)/crypt.c
	$(CRULE_OPT)

des_cbc_encrypt.o: $(DES_SRCDIR)/cbc_encrypt.c
	$(CRULE_OPT)

des_pcbc_encrypt.o: $(DES_SRCDIR)/pcbc_encrypt.c
	$(CRULE_OPT)

des_cksum.o: $(DES_SRCDIR)/cksum.c
	$(CRULE_OPT)

des_new_rnd_key.o: $(DES_SRCDIR)/new_rnd_key.c
	$(CRULE_OPT)

des_key_sched.o: $(DES_SRCDIR)/key_sched.c
	$(CRULE_OPT)

des_debug_decl.o: $(DES_SRCDIR)/debug_decl.c
	$(CRULE_OPT)

des_quad_cksum.o: $(DES_SRCDIR)/quad_cksum.c
	$(CRULE_OPT)

des_key_parity.o: $(DES_SRCDIR)/key_parity.c
	$(CRULE_OPT)

des_weak_key.o: $(DES_SRCDIR)/weak_key.c
	$(CRULE_OPT)

des_strng_to_key.o: $(DES_SRCDIR)/strng_to_key.c
	$(CRULE_OPT)

des_misc.o: $(DES_SRCDIR)/misc.c
	$(CRULE_OPT)

des_util.o: $(DES_SRCDIR)/util.c
	$(CRULE_OPT)

comerr_error_msg.o: $(COMERR_SRCDIR)/error_msg.c
	$(CRULE_OPT)

comerr_et_name.o: $(COMERR_SRCDIR)/et_name.c
	$(CRULE_OPT)

comerr_com_err.o: $(COMERR_SRCDIR)/com_err.c
	$(CRULE_OPT)

util_casestrcpy.o: $(UTIL_SRCDIR)/casestrcpy.c
	$(CRULE_OPT)

util_assert.o: $(UTIL_SRCDIR)/assert.c
	$(CRULE_OPT)

util_base64.o: $(UTIL_SRCDIR)/base64.c
	$(CRULE_OPT)

lwp.o: $(LWP_SRCDIR)/lwp.c
	$(CRULE_OPT)

lwp_fasttime.o: $(LWP_SRCDIR)/fasttime.c
	$(CRULE_OPT)

lwp_iomgr.o: $(LWP_SRCDIR)/iomgr.c
	$(CRULE_OPT)

lwp_preempt.o: $(LWP_SRCDIR)/preempt.c
	$(CRULE_OPT)

$(LIBAFSRPC_CC) $(LIBAFSRPC_ALL_CFLAGS) $(CFLAGS-$@)
lwp_process.o:

lwp_threadname.o: $(LWP_SRCDIR)/threadname.c
	$(CRULE_OPT)

lwp_timer.o: $(LWP_SRCDIR)/timer.
	$(CRULE_OPT)

lwp_waitkey.o: $(LWP_SRCDIR)/waitkey.c
	$(CRULE_OPT)

sys_syscall.o: $(SYS_SRCDIR)/syscall.s
	case "$(SYS_NAME)" in \
	     sun4x_5* | sunx86_5*) \
		/usr/ccs/lib/cpp  ${SFLAGS} $(SYS_SRCDIR)/syscall.s syscall.ss; \
		as -o sys_syscall.o syscall.ss;	\
		$(RM) syscall.ss;; \
	 sgi_* | *_darwin_* ) \
                ${CC} ${CFLAGS} -o sys_syscall.o -c $(SYS_SRCDIR)/syscall.s;; \
	 alpha_dux?? ) \
		${AS} -P ${CFLAGS} -D_NO_PROTO -DMACH -DOSF -nostdinc -traditional -DASSEMBLER $(SYS_SRCDIR)/syscall.s; \
		${AS} -o sys_syscall.o syscall.i; \
		$(RM) -f syscall.ss syscall.i;; \
	 *bsd* ) \
		touch sys_syscall.o ;; \
	 *) \
		/lib/cpp  ${SFLAGS} $(SYS_SRCDIR)/syscall.s syscall.ss; \
		as -o sys_syscall.o syscall.ss;		\
		$(RM) syscall.ss;;				\
	esac

rxstat.o: $(RXSTAT_SRCDIR)/rxstat.c
	$(CRULE_OPT)

rxstat.cs.o: $(RXSTAT_OBJDIR)/rxstat.cs.c
	$(CRULE_OPT)

rxstat.ss.o: $(RXSTAT_OBJDIR)/rxstat.ss.c
	$(CRULE_OPT)

rxstat.xdr.o: $(RXSTAT_OBJDIR)//rxstat.xdr.c
	$(CRULE_OPT)

afsint.cs.o: $(FSINT_OBJDIR)/afsint.cs.c
	$(CRULE_OPT)

afsint.xdr.o: $(FSINT_OBJDIR)/afsint.xdr.c
	$(CRULE_OPT)

afscbint.cs.o: $(FSINT_OBJDIR)/afscbint.cs.c
	$(CRULE_OPT)

afscbint.xdr.o: $(FSINT_OBJDIR)/afscbint.xdr.c
	$(CRULE_OPT)

afsaux.o: $(FSINT_SRCDIR)/afsaux.c
	$(CRULE_OPT)
