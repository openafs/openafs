# Copyright 2006, Sine Nomine Associates and others.
# All Rights Reserved.
# 
# This software has been released under the terms of the IBM Public
# License.  For details, see the LICENSE file in the top-level source
# directory or online at http://www.openafs.org/dl/license10.html


OSI_SRCDIR=$(TOP_SRCDIR)/osi
TRACE_SRCDIR=$(TOP_SRCDIR)/trace
LWP_SRCDIR=$(TOP_SRCDIR)/lwp
COMERR_SRCDIR=$(TOP_SRCDIR)/comerr
UTIL_SRCDIR=$(TOP_SRCDIR)/util

OSI_OBJDIR=$(TOP_OBJDIR)/src/osi
TRACE_OBJDIR=$(TOP_OBJDIR)/src/trace
LWP_OBJDIR=$(TOP_OBJDIR)/src/lwp
COMERR_OBJDIR=$(TOP_OBJDIR)/src/comerr
UTIL_OBJDIR=$(TOP_OBJDIR)/src/util


LIBTRACE_VERSION_MAJOR = 1
LIBTRACE_VERSION_MINOR = 0
LIBTRACE_DEPS_libosi_trace_consumer_shlib_pthread = \
	${MT_LIBS} ${OSI_LIBS} -L$(TOP_OBJDIR)/lib \
	-losi_trace_common-$(LIBTRACE_BUILDING_THR)-$(LIBTRACE_BUILDING_BIT) \
	-losi-$(LIBTRACE_BUILDING_THR)-ni-$(LIBTRACE_BUILDING_BIT) \
	$(NULL)

# core libtrace build objects
# for all builds, kernel, ukernel, and userspace

LIBTRACE_CFLAGS_COMMON = 
LIBTRACE_OBJS_COMMON = \
	$(NULL)

LIBTRACE_CFLAGS_kernelspace = 
LIBTRACE_OBJS_kernelspace = \
	$(NULL)

LIBTRACE_CFLAGS_kernel = 
LIBTRACE_OBJS_kernel = \
	$(NULL)

LIBTRACE_CFLAGS_userspace = 
LIBTRACE_OBJS_userspace = \
	$(NULL)

LIBTRACE_CFLAGS_ukernel = 
LIBTRACE_OBJS_ukernel = \
	$(NULL)

LIBTRACE_CFLAGS_libosi_trace_consumer = ${COMMON_INCLUDE} ${CFLAGS} ${OSI_CFLAGS} ${ARCHFLAGS} ${XCFLAGS} -DOSI_TRACEPOINT_DISABLE
LIBTRACE_OBJS_libosi_trace_consumer = \
	common_init.o \
	consumer.o \
	consumer_activation.o \
	consumer_cache_binary.o \
	consumer_cache_generator.o \
	consumer_cache_gen_coherency.o \
	consumer_cache_probe_info.o \
	consumer_cache_probe_value.o \
	consumer_cache_ptr_vec.o \
	consumer_config.o \
	consumer_cursor.o \
	consumer_directory.o \
	consumer_gen_rgy.o \
	consumer_gen_rgy_mail.o \
	consumer_i2n.o \
	consumer_i2n_mail.o \
	consumer_i2n_thread.o \
	consumer_init.o \
	consumer_local_namespace.o \
	consumer_module.o \
	consumer_record_queue.o \
	consumer_record_fsa.o \
	consumer_thread.o \
	mail_init.o \
	mail_sys.o \
	mail_common.o \
	mail_handler.o \
	mail_header.o \
	mail_thread.o \
	mail_rpc.o \
	mail_xid.o \
	userspace_gen_rgy.o \
	userspace_syscall.o \
	userspace_syscall_probe.o \
	$(NULL)


# userspace threading model-specific build objects

LIBTRACE_CFLAGS_userspace_lwp = 
LIBTRACE_OBJS_userspace_lwp = \
	$(NULL)

LIBTRACE_CFLAGS_ukernel_lwp = 
LIBTRACE_OBJS_ukernel_lwp = \
	$(NULL)

LIBTRACE_CFLAGS_libosi_trace_consumer_lwp = 
LIBTRACE_OBJS_libosi_trace_consumer_lwp = \
	$(NULL)

LIBTRACE_CFLAGS_userspace_pthread = ${MT_CFLAGS}
LIBTRACE_OBJS_userspace_pthread = \
	$(NULL)

LIBTRACE_CFLAGS_ukernel_pthread = 
LIBTRACE_OBJS_ukernel_pthread = \
	$(NULL)

LIBTRACE_CFLAGS_libosi_trace_consumer_pthread = 
LIBTRACE_OBJS_libosi_trace_consumer_pthread = \
	$(NULL)


LIBTRACE_CFLAGS_COMMON_32 = 
LIBTRACE_OBJS_COMMON_32 = \
	$(NULL)

LIBTRACE_CFLAGS_kernelspace_32 = 
LIBTRACE_OBJS_kernelspace_32 = \
	$(NULL)

LIBTRACE_CFLAGS_kernel_32 = 
LIBTRACE_OBJS_kernel_32 = \
	$(NULL)

LIBTRACE_CFLAGS_userspace_32 = 
LIBTRACE_OBJS_userspace_32 = \
	$(NULL)

LIBTRACE_CFLAGS_ukernel_32 = 
LIBTRACE_OBJS_ukernel_32 = \
	$(NULL)

LIBTRACE_CFLAGS_libosi_trace_consumer_32 = 
LIBTRACE_OBJS_libosi_trace_consumer_32 = \
	$(NULL)


LIBTRACE_CFLAGS_COMMON_64 = 
LIBTRACE_OBJS_COMMON_64 = \
	$(NULL)

LIBTRACE_CFLAGS_kernelspace_64 = 
LIBTRACE_OBJS_kernelspace_64 = \
	$(NULL)

LIBTRACE_CFLAGS_kernel_64 = 
LIBTRACE_OBJS_kernel_64 = \
	$(NULL)

LIBTRACE_CFLAGS_userspace_64 = 
LIBTRACE_OBJS_userspace_64 = \
	$(NULL)

LIBTRACE_CFLAGS_ukernel_64 = 
LIBTRACE_OBJS_ukernel_64 = \
	$(NULL)

LIBTRACE_CFLAGS_libosi_trace_consumer_64 = 
LIBTRACE_OBJS_libosi_trace_consumer_64 = \
	$(NULL)

# archive versus shlib stuff

LIBTRACE_CFLAGS_libosi_trace_consumer_archive =
LIBTRACE_OBJS_libosi_trace_consumer_archive = \
	$(NULL)

LIBTRACE_CFLAGS_libosi_trace_consumer_shlib = ${SHLIB_CFLAGS}
LIBTRACE_OBJS_libosi_trace_consumer_shlib = \
	$(NULL)


# build rules for OS Interface library

consumer.o: $(TRACE_SRCDIR)/consumer/consumer.c
	$(CRULE_OPT)

consumer_activation.o: $(TRACE_SRCDIR)/consumer/activation.c
	$(CRULE_OPT)

consumer_cache_binary.o: $(TRACE_SRCDIR)/consumer/cache/binary.c
	$(CRULE_OPT)

consumer_cache_generator.o: $(TRACE_SRCDIR)/consumer/cache/generator.c
	$(CRULE_OPT)

consumer_cache_gen_coherency.o: $(TRACE_SRCDIR)/consumer/cache/gen_coherency.c
	$(CRULE_OPT)

consumer_cache_probe_info.o: $(TRACE_SRCDIR)/consumer/cache/probe_info.c
	$(CRULE_OPT)

consumer_cache_probe_value.o: $(TRACE_SRCDIR)/consumer/cache/probe_value.c
	$(CRULE_OPT)

consumer_cache_ptr_vec.o: $(TRACE_SRCDIR)/consumer/cache/ptr_vec.c
	$(CRULE_OPT)

consumer_config.o: $(TRACE_SRCDIR)/consumer/config.c
	$(CRULE_OPT)

consumer_cursor.o: $(TRACE_SRCDIR)/consumer/cursor.c
	$(CRULE_OPT)

consumer_directory.o: $(TRACE_SRCDIR)/consumer/directory.c
	$(CRULE_OPT)

consumer_gen_rgy.o: $(TRACE_SRCDIR)/consumer/gen_rgy.c
	$(CRULE_OPT)

consumer_gen_rgy_mail.o: $(TRACE_SRCDIR)/consumer/gen_rgy_mail.c
	$(CRULE_OPT)

consumer_i2n.o: $(TRACE_SRCDIR)/consumer/i2n.c
	$(CRULE_OPT)

consumer_i2n_mail.o: $(TRACE_SRCDIR)/consumer/i2n_mail.c
	$(CRULE_OPT)

consumer_i2n_thread.o: $(TRACE_SRCDIR)/consumer/i2n_thread.c
	$(CRULE_OPT)

consumer_init.o: $(TRACE_SRCDIR)/consumer/init.c
	$(CRULE_OPT)

consumer_local_namespace.o: $(TRACE_SRCDIR)/consumer/local_namespace.c
	$(CRULE_OPT)

consumer_module.o: $(TRACE_SRCDIR)/consumer/module.c
	$(CRULE_OPT)

consumer_record_queue.o: $(TRACE_SRCDIR)/consumer/record_queue.c
	$(CRULE_OPT)

consumer_record_fsa.o: $(TRACE_SRCDIR)/consumer/record_fsa.c
	$(CRULE_OPT)

consumer_thread.o: $(TRACE_SRCDIR)/consumer/consumer_thread.c
	$(CRULE_OPT)

userspace_gen_rgy.o: $(TRACE_SRCDIR)/USERSPACE/gen_rgy.c
	$(CRULE_OPT)

userspace_syscall.o: $(TRACE_SRCDIR)/USERSPACE/syscall.c
	$(CRULE_OPT)

userspace_syscall_probe.o: $(TRACE_SRCDIR)/USERSPACE/syscall_probe.c
	$(CRULE_OPT)

mail_sys.o: $(TRACE_SRCDIR)/USERSPACE/mail.c
	$(CRULE_OPT)

mail_init.o: $(TRACE_SRCDIR)/mail/init.c
	$(CRULE_OPT)

mail_rpc.o: $(TRACE_SRCDIR)/mail/rpc.c
	$(CRULE_OPT)

mail_xid.o: $(TRACE_SRCDIR)/mail/xid.c
	$(CRULE_OPT)

mail_common.o: $(TRACE_SRCDIR)/mail/common.c
	$(CRULE_OPT)

mail_handler.o: $(TRACE_SRCDIR)/mail/handler.c
	$(CRULE_OPT)

mail_header.o: $(TRACE_SRCDIR)/mail/header.c
	$(CRULE_OPT)

mail_thread.o: $(TRACE_SRCDIR)/PTHREAD/mail_thread.c
	$(CRULE_OPT)

common_init.o: $(TRACE_SRCDIR)/common/init.c
	$(CRULE_OPT)

common_options.o: $(TRACE_SRCDIR)/common/options.c
	$(CRULE_OPT)

common_record_inline.o: $(TRACE_SRCDIR)/common/record_inline.c
	$(CRULE_OPT)

