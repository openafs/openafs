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


LIBOSI_VERSION_MAJOR = 1
LIBOSI_VERSION_MINOR = 0
LIBOSI_DEPS_libosi_shlib_pthread = ${MT_LIBS} ${OSI_LIBS}

# core libosi build objects
# for all builds, kernel, ukernel, and userspace

LIBOSI_CFLAGS_COMMON = 
LIBOSI_OBJS_COMMON = \
	osi.o \
	osi_common_cache.o \
	osi_common_condvar_options.o \
	osi_common_counter.o \
	osi_common_counter_inline.o \
	osi_common_event_lock.o \
	osi_common_libosi_options.o \
	osi_common_mutex_options.o \
	osi_common_object_cache.o \
	osi_common_object_cache_options.o \
	osi_common_object_init.o \
	osi_common_object_init_inline.o \
	osi_common_options.o \
	osi_common_refcnt_inline.o \
	osi_common_rwlock_options.o \
	osi_common_shlock_options.o \
	osi_common_spinlock_options.o \
	osi_common_spin_rwlock_options.o \
	osi_common_syscall.o \
	osi_common_thread_event.o \
	osi_common_thread_event_inline.o \
	osi_common_thread_option.o \
	osi_common_util.o \
	osi_legacy_counter.o \
	osi_legacy_counter_inline.o \
	osi_legacy_refcnt.o \
	osi_legacy_refcnt_inline.o \
	osi_legacy_spinlock.o \
	osi_legacy_spin_rwlock.o \
	osi_legacy_strnlen.o \
	osi_legacy_strlcat.o \
	osi_legacy_strlcpy.o \
	$(NULL)

LIBOSI_CFLAGS_kernelspace = 
LIBOSI_OBJS_kernelspace = \
	osi_common_kutil.o \
	$(NULL)

LIBOSI_CFLAGS_kernel = 
LIBOSI_OBJS_kernel = \
	$(NULL)

LIBOSI_CFLAGS_userspace = 
LIBOSI_OBJS_userspace = \
	osi_common_uutil.o \
	osi_common_uproc_inline.o \
	osi_common_usignal_inline.o \
	osi_common_usyscall.o \
	osi_common_usyscall_probe.o \
	$(NULL)

LIBOSI_CFLAGS_ukernel = 
LIBOSI_OBJS_ukernel = \
	$(NULL)

LIBOSI_CFLAGS_libosi = ${COMMON_INCLUDE} ${CFLAGS} ${OSI_CFLAGS} ${ARCHFLAGS} ${XCFLAGS}
LIBOSI_OBJS_libosi = \
	comerr.o \
	comerr_et_name.o \
	comerr_error_msg.o \
	lwp_shlock.o \
	osi_err.o \
	util_casestrcpy.o \
	$(NULL)


# userspace threading model-specific build objects

LIBOSI_CFLAGS_userspace_lwp = 
LIBOSI_OBJS_userspace_lwp = \
	osi_lwp_condvar.o \
	osi_lwp_mutex.o \
	osi_lwp_rwlock.o \
	osi_lwp_shlock.o \
	osi_lwp_thread.o \
	$(NULL)

LIBOSI_CFLAGS_ukernel_lwp = 
LIBOSI_OBJS_ukernel_lwp = \
	$(NULL)

LIBOSI_CFLAGS_libosi_lwp = 
LIBOSI_OBJS_libosi_lwp = \
	$(NULL)

LIBOSI_CFLAGS_userspace_pthread = ${MT_CFLAGS}
LIBOSI_OBJS_userspace_pthread = \
	osi_common_mem_local.o \
	osi_common_mem_local_inline.o \
	osi_pthread_shlock.o \
	osi_pthread_spinlock.o \
	osi_pthread_thread.o \
	osi_pthread_umem_local.o \
	$(NULL)

LIBOSI_CFLAGS_ukernel_pthread = 
LIBOSI_OBJS_ukernel_pthread = \
	$(NULL)

LIBOSI_CFLAGS_libosi_pthread = 
LIBOSI_OBJS_libosi_pthread = \
	$(NULL)


# osi trace build objects for
# all build environments, kernel, ukernel, and userspace

LIBOSI_CFLAGS_COMMON_inst =
LIBOSI_OBJS_COMMON_inst = \
	osi_tracepoint_table.o \
	trace_common_init.o \
	trace_common_options.o \
	trace_common_record_inline.o \
	trace_event_event.o \
	trace_event_function.o \
	trace_generator.o \
	trace_generator_activation.o \
	trace_generator_cursor.o \
	trace_generator_directory.o \
	trace_generator_init.o \
	trace_generator_module.o \
	trace_generator_probe.o \
	trace_generator_tracepoint_inline.o \
	trace_mail_common.o \
	trace_mail_header.o \
	trace_mail_init.o \
	$(NULL)

LIBOSI_CFLAGS_kernelspace_inst = 
LIBOSI_OBJS_kernelspace_inst = \
	trace_event_icl.o \
	trace_event_usertrap.o \
	trace_generator_buffer.o \
	trace_kernel_config.o \
	trace_kernel_cursor.o \
	trace_kernel_gen_rgy.o \
	trace_kernel_gen_rgy_inline.o \
	trace_kernel_module_sys.o \
	trace_kernel_postmaster.o \
        trace_kernel_query.o \
        trace_kernel_syscall.o \
	$(NULL)

LIBOSI_CFLAGS_kernel_inst = 
LIBOSI_OBJS_kernel_inst = \
	$(NULL)

LIBOSI_CFLAGS_userspace_inst = 
LIBOSI_OBJS_userspace_inst = \
	$(NULL)

# totally disable tracing on UKERNEL for now
# due to ukernel's complete brokenness
# (e.g. no threading model is specified)
LIBOSI_CFLAGS_ukernel_inst = -DOSI_TRACE_DISABLE
LIBOSI_OBJS_ukernel_inst = \
	$(NULL)

LIBOSI_CFLAGS_libosi_inst = 
LIBOSI_OBJS_libosi_inst = \
	trace_generator_directory_mail.o \
	trace_generator_module_mail.o \
	trace_generator_probe_mail.o \
	trace_mail_handler.o \
	trace_mail_rpc.o \
	trace_mail_xid.o \
	trace_$(LIBOSI_BUILDING_THR)_mail_thread.o \
	trace_userspace_gen_rgy.o \
	trace_userspace_mail.o \
	trace_userspace_syscall.o \
	trace_userspace_syscall_probe.o \
	$(NULL)

LIBOSI_CFLAGS_COMMON_ni = -DOSI_TRACE_DISABLE
LIBOSI_OBJS_COMMON_ni = \
	$(NULL)

LIBOSI_CFLAGS_kernelspace_ni = 
LIBOSI_OBJS_kernelspace_ni = \
	$(NULL)

LIBOSI_CFLAGS_kernel_ni = 
LIBOSI_OBJS_kernel_ni = \
	$(NULL)

LIBOSI_CFLAGS_userspace_ni = 
LIBOSI_OBJS_userspace_ni = \
	$(NULL)

LIBOSI_CFLAGS_ukernel_ni =
LIBOSI_OBJS_ukernel_ni = \
	$(NULL)

LIBOSI_CFLAGS_libosi_ni = 
LIBOSI_OBJS_libosi_ni = \
	$(NULL)


LIBOSI_CFLAGS_COMMON_32 = 
LIBOSI_OBJS_COMMON_32 = \
	$(NULL)

LIBOSI_CFLAGS_kernelspace_32 = 
LIBOSI_OBJS_kernelspace_32 = \
	$(NULL)

LIBOSI_CFLAGS_kernel_32 = 
LIBOSI_OBJS_kernel_32 = \
	$(NULL)

LIBOSI_CFLAGS_userspace_32 = 
LIBOSI_OBJS_userspace_32 = \
	$(NULL)

LIBOSI_CFLAGS_ukernel_32 = 
LIBOSI_OBJS_ukernel_32 = \
	$(NULL)

LIBOSI_CFLAGS_libosi_32 = 
LIBOSI_OBJS_libosi_32 = \
	$(NULL)


LIBOSI_CFLAGS_COMMON_64 = 
LIBOSI_OBJS_COMMON_64 = \
	$(NULL)

LIBOSI_CFLAGS_kernelspace_64 = 
LIBOSI_OBJS_kernelspace_64 = \
	$(NULL)

LIBOSI_CFLAGS_kernel_64 = 
LIBOSI_OBJS_kernel_64 = \
	$(NULL)

LIBOSI_CFLAGS_userspace_64 = 
LIBOSI_OBJS_userspace_64 = \
	$(NULL)

LIBOSI_CFLAGS_ukernel_64 = 
LIBOSI_OBJS_ukernel_64 = \
	$(NULL)

LIBOSI_CFLAGS_libosi_64 = 
LIBOSI_OBJS_libosi_64 = \
	$(NULL)

# archive versus shlib stuff

LIBOSI_CFLAGS_libosi_archive =
LIBOSI_OBJS_libosi_archive = \
	$(NULL)

LIBOSI_CFLAGS_libosi_shlib = ${SHLIB_CFLAGS}
LIBOSI_OBJS_libosi_shlib = \
	$(NULL)


# build rules for OS Interface library

osi.o: $(OSI_SRCDIR)/osi.c
	$(CRULE_OPT)
osi_err.o: $(OSI_OBJDIR)/osi_err.c
	$(CRULE_OPT)
osi_common_cache.o: $(OSI_SRCDIR)/COMMON/cache.c
	$(CRULE_OPT)
osi_common_condvar_options.o: $(OSI_SRCDIR)/COMMON/condvar_options.c
	$(CRULE_OPT)
osi_common_counter.o: $(OSI_SRCDIR)/COMMON/counter.c
	$(CRULE_OPT)
osi_common_counter_inline.o: $(OSI_SRCDIR)/COMMON/counter_inline.c
	$(CRULE_OPT)
osi_common_event_lock.o: $(OSI_SRCDIR)/COMMON/event_lock.c
	$(CRULE_OPT)
osi_common_kutil.o: $(OSI_SRCDIR)/COMMON/kutil.c
	$(CRULE_OPT)
osi_common_libosi_options.o: $(OSI_SRCDIR)/COMMON/libosi_options.c
	$(CRULE_OPT)
osi_common_mem_local.o: $(OSI_SRCDIR)/COMMON/mem_local.c
	$(CRULE_OPT)
osi_common_mem_local_inline.o: $(OSI_SRCDIR)/COMMON/mem_local_inline.c
	$(CRULE_OPT)
osi_common_mutex_options.o: $(OSI_SRCDIR)/COMMON/mutex_options.c
	$(CRULE_OPT)
osi_common_object_cache.o: $(OSI_SRCDIR)/COMMON/object_cache.c
	$(CRULE_OPT)
osi_common_object_cache_options.o: $(OSI_SRCDIR)/COMMON/object_cache_options.c
	$(CRULE_OPT)
osi_common_object_init.o: $(OSI_SRCDIR)/COMMON/object_init.c
	$(CRULE_OPT)
osi_common_object_init_inline.o: $(OSI_SRCDIR)/COMMON/object_init_inline.c
	$(CRULE_OPT)
osi_common_options.o: $(OSI_SRCDIR)/COMMON/options.c
	$(CRULE_OPT)
osi_common_refcnt_inline.o: $(OSI_SRCDIR)/COMMON/refcnt_inline.c
	$(CRULE_OPT)
osi_common_rwlock_options.o: $(OSI_SRCDIR)/COMMON/rwlock_options.c
	$(CRULE_OPT)
osi_common_shlock_options.o: $(OSI_SRCDIR)/COMMON/shlock_options.c
	$(CRULE_OPT)
osi_common_spinlock_options.o: $(OSI_SRCDIR)/COMMON/spinlock_options.c
	$(CRULE_OPT)
osi_common_spin_rwlock_options.o: $(OSI_SRCDIR)/COMMON/spin_rwlock_options.c
	$(CRULE_OPT)
osi_common_syscall.o: $(OSI_SRCDIR)/COMMON/syscall.c
	$(CRULE_OPT)
osi_common_thread_event.o: $(OSI_SRCDIR)/COMMON/thread_event.c
	$(CRULE_OPT)
osi_common_thread_event_inline.o: $(OSI_SRCDIR)/COMMON/thread_event_inline.c
	$(CRULE_OPT)
osi_common_thread_option.o: $(OSI_SRCDIR)/COMMON/thread_option.c
	$(CRULE_OPT)
osi_common_uproc_inline.o: $(OSI_SRCDIR)/COMMON/uproc_inline.c
	$(CRULE_OPT)
osi_common_usignal_inline.o: $(OSI_SRCDIR)/COMMON/usignal_inline.c
	$(CRULE_OPT)
osi_common_usyscall.o: $(OSI_SRCDIR)/COMMON/usyscall.c
	$(CRULE_OPT)
osi_common_usyscall_probe.o: $(OSI_SRCDIR)/COMMON/usyscall_probe.c
	$(CRULE_OPT)
osi_common_util.o: $(OSI_SRCDIR)/COMMON/util.c
	$(CRULE_OPT)
osi_common_uutil.o: $(OSI_SRCDIR)/COMMON/uutil.c
	$(CRULE_OPT)
osi_legacy_counter.o: $(OSI_SRCDIR)/LEGACY/counter.c
	$(CRULE_OPT)
osi_legacy_counter_inline.o: $(OSI_SRCDIR)/LEGACY/counter_inline.c
	$(CRULE_OPT)
osi_legacy_kcondvar.o: $(OSI_SRCDIR)/LEGACY/kcondvar.c
	$(CRULE_OPT)
osi_legacy_kmutex.o: $(OSI_SRCDIR)/LEGACY/kmutex.c
	$(CRULE_OPT)
osi_legacy_krwlock.o: $(OSI_SRCDIR)/LEGACY/krwlock.c
	$(CRULE_OPT)
osi_legacy_kshlock.o: $(OSI_SRCDIR)/LEGACY/kshlock.c
	$(CRULE_OPT)
osi_legacy_ktime_inline.o: $(OSI_SRCDIR)/LEGACY/ktime_inline.c
	$(CRULE_OPT)
osi_legacy_mem_align.o: $(OSI_SRCDIR)/LEGACY/mem_align.c
	$(CRULE_OPT)
osi_legacy_object_cache.o: $(OSI_SRCDIR)/LEGACY/object_cache.c
	$(CRULE_OPT)
osi_legacy_refcnt.o: $(OSI_SRCDIR)/LEGACY/refcnt.c
	$(CRULE_OPT)
osi_legacy_refcnt_inline.o: $(OSI_SRCDIR)/LEGACY/refcnt_inline.c
	$(CRULE_OPT)
osi_legacy_spinlock.o: $(OSI_SRCDIR)/LEGACY/spinlock.c
	$(CRULE_OPT)
osi_legacy_spin_rwlock.o: $(OSI_SRCDIR)/LEGACY/spin_rwlock.c
	$(CRULE_OPT)
osi_legacy_strnlen.o: $(OSI_SRCDIR)/LEGACY/strnlen.c
	$(CRULE_OPT)
osi_legacy_strlcat.o: $(OSI_SRCDIR)/LEGACY/strlcat.c
	$(CRULE_OPT)
osi_legacy_strlcpy.o: $(OSI_SRCDIR)/LEGACY/strlcpy.c
	$(CRULE_OPT)
osi_legacy_utime_inline.o: $(OSI_SRCDIR)/LEGACY/utime_inline.c
	$(CRULE_OPT)
osi_lwp_condvar.o: $(OSI_SRCDIR)/LWP/condvar.c
	$(CRULE_OPT)
osi_lwp_mutex.o: $(OSI_SRCDIR)/LWP/mutex.c
	$(CRULE_OPT)
osi_lwp_rwlock.o: $(OSI_SRCDIR)/LWP/rwlock.c
	$(CRULE_OPT)
osi_lwp_shlock.o: $(OSI_SRCDIR)/LWP/shlock.c
	$(CRULE_OPT)
osi_lwp_thread.o: $(OSI_SRCDIR)/LWP/thread.c
	$(CRULE_OPT)
osi_native_kcache.o: $(OSI_SRCDIR)/$(MKAFS_OSTYPE)/kcache.c
	$(CRULE_OPT)
osi_native_kcondvar.o: $(OSI_SRCDIR)/$(MKAFS_OSTYPE)/kcondvar.c
	$(CRULE_OPT)
osi_native_kcondvar_inline.o: $(OSI_SRCDIR)/$(MKAFS_OSTYPE)/kcondvar_inline.c
	$(CRULE_OPT)
osi_native_kcpu.o: $(OSI_SRCDIR)/$(MKAFS_OSTYPE)/kcpu.c
	$(CRULE_OPT)
osi_native_kmem.o: $(OSI_SRCDIR)/$(MKAFS_OSTYPE)/kmem.c
	$(CRULE_OPT)
osi_native_kmutex.o: $(OSI_SRCDIR)/$(MKAFS_OSTYPE)/kmutex.c
	$(CRULE_OPT)
osi_native_kproc.o: $(OSI_SRCDIR)/$(MKAFS_OSTYPE)/kproc.c
	$(CRULE_OPT)
osi_native_krwlock.o: $(OSI_SRCDIR)/$(MKAFS_OSTYPE)/krwlock.c
	$(CRULE_OPT)
osi_native_krwlock_inline.o: $(OSI_SRCDIR)/$(MKAFS_OSTYPE)/krwlock_inline.c
	$(CRULE_OPT)
osi_native_kspinlock.o: $(OSI_SRCDIR)/$(MKAFS_OSTYPE)/kspinlock.c
	$(CRULE_OPT)
osi_native_kspin_rwlock.o: $(OSI_SRCDIR)/$(MKAFS_OSTYPE)/kspin_rwlock.c
	$(CRULE_OPT)
osi_native_kspin_rwlock_inline.o: $(OSI_SRCDIR)/$(MKAFS_OSTYPE)/kspin_rwlock_inline.c
	$(CRULE_OPT)
osi_native_kthread.o: $(OSI_SRCDIR)/$(MKAFS_OSTYPE)/kthread.c
	$(CRULE_OPT)
osi_native_kutil.o: $(OSI_SRCDIR)/$(MKAFS_OSTYPE)/kutil.c
	$(CRULE_OPT)
osi_native_ucache.o: $(OSI_SRCDIR)/$(MKAFS_OSTYPE)/ucache.c
	$(CRULE_OPT)
osi_native_ucpu.o: $(OSI_SRCDIR)/$(MKAFS_OSTYPE)/ucpu.c
	$(CRULE_OPT)
osi_native_usyscall.o: $(OSI_SRCDIR)/$(MKAFS_OSTYPE)/usyscall.c
	$(CRULE_OPT)
osi_native_utime.o: $(OSI_SRCDIR)/$(MKAFS_OSTYPE)/utime.c
	$(CRULE_OPT)
osi_native_utime_inline.o: $(OSI_SRCDIR)/$(MKAFS_OSTYPE)/utime_inline.c
	$(CRULE_OPT)
osi_native_utime_approx.o: $(OSI_SRCDIR)/$(MKAFS_OSTYPE)/utime_approx.c
	$(CRULE_OPT)
osi_native_utime_approx_inline.o: $(OSI_SRCDIR)/$(MKAFS_OSTYPE)/utime_approx_inline.c
	$(CRULE_OPT)
osi_pthread_shlock.o: $(OSI_SRCDIR)/PTHREAD/shlock.c
	$(CRULE_OPT)
osi_pthread_spinlock.o: $(OSI_SRCDIR)/PTHREAD/spinlock.c
	$(CRULE_OPT)
osi_pthread_thread.o: $(OSI_SRCDIR)/PTHREAD/thread.c
	$(CRULE_OPT)
osi_pthread_umem_local.o: $(OSI_SRCDIR)/PTHREAD/umem_local.c
	$(CRULE_OPT)
osi_tracepoint_table.o: $(OSI_OBJDIR)/osi_tracepoint_table.c
	$(CRULE_OPT)


# build rules for OSI Trace Generator library

trace_common_init.o: $(TRACE_SRCDIR)/common/init.c
	$(CRULE_OPT)
trace_common_options.o: $(TRACE_SRCDIR)/common/options.c
	$(CRULE_OPT)
trace_common_record_inline.o: $(TRACE_SRCDIR)/common/record_inline.c
	$(CRULE_OPT)
trace_event_event.o: $(TRACE_SRCDIR)/event/event.c
	$(CRULE_OPT)
trace_event_function.o: $(TRACE_SRCDIR)/event/function.c
	$(CRULE_OPT)
trace_event_icl.o: $(TRACE_SRCDIR)/event/icl.c
	$(CRULE_OPT)
trace_event_usertrap.o: $(TRACE_SRCDIR)/event/usertrap.c
	$(CRULE_OPT)
trace_generator.o: $(TRACE_SRCDIR)/generator/generator.c
	$(CRULE_OPT)
trace_generator_activation.o: $(TRACE_SRCDIR)/generator/activation.c
	$(CRULE_OPT)
trace_generator_buffer.o: $(TRACE_SRCDIR)/generator/buffer.c
	$(CRULE_OPT)
trace_generator_cursor.o: $(TRACE_SRCDIR)/generator/cursor.c
	$(CRULE_OPT)
trace_generator_directory.o: $(TRACE_SRCDIR)/generator/directory.c
	$(CRULE_OPT)
trace_generator_directory_mail.o: $(TRACE_SRCDIR)/generator/directory_mail.c
	$(CRULE_OPT)
trace_generator_init.o: $(TRACE_SRCDIR)/generator/init.c
	$(CRULE_OPT)
trace_generator_module.o: $(TRACE_SRCDIR)/generator/module.c
	$(CRULE_OPT)
trace_generator_module_mail.o: $(TRACE_SRCDIR)/generator/module_mail.c
	$(CRULE_OPT)
trace_generator_probe.o: $(TRACE_SRCDIR)/generator/probe.c
	$(CRULE_OPT)
trace_generator_probe_mail.o: $(TRACE_SRCDIR)/generator/probe_mail.c
	$(CRULE_OPT)
trace_generator_tracepoint_inline.o: $(TRACE_SRCDIR)/generator/tracepoint_inline.c
	$(CRULE_OPT)
trace_kernel_config.o: $(TRACE_SRCDIR)/KERNEL/config.c
	$(CRULE_OPT)
trace_kernel_cursor.o: $(TRACE_SRCDIR)/KERNEL/cursor.c
	$(CRULE_OPT)
trace_kernel_gen_rgy.o: $(TRACE_SRCDIR)/KERNEL/gen_rgy.c
	$(CRULE_OPT)
trace_kernel_gen_rgy_inline.o: $(TRACE_SRCDIR)/KERNEL/gen_rgy_inline.c
	$(CRULE_OPT)
trace_kernel_module_sys.o: $(TRACE_SRCDIR)/KERNEL/module.c
	$(CRULE_OPT)
trace_kernel_postmaster.o: $(TRACE_SRCDIR)/KERNEL/postmaster.c
	$(CRULE_OPT)
trace_kernel_query.o: $(TRACE_SRCDIR)/KERNEL/query.c
	$(CRULE_OPT)
trace_kernel_syscall.o: $(TRACE_SRCDIR)/KERNEL/syscall.c
	$(CRULE_OPT)
trace_mail_common.o: $(TRACE_SRCDIR)/mail/common.c
	$(CRULE_OPT)
trace_mail_handler.o: $(TRACE_SRCDIR)/mail/handler.c
	$(CRULE_OPT)
trace_mail_header.o: $(TRACE_SRCDIR)/mail/header.c
	$(CRULE_OPT)
trace_mail_init.o: $(TRACE_SRCDIR)/mail/init.c
	$(CRULE_OPT)
trace_mail_rpc.o: $(TRACE_SRCDIR)/mail/rpc.c
	$(CRULE_OPT)
trace_mail_xid.o: $(TRACE_SRCDIR)/mail/xid.c
	$(CRULE_OPT)
trace_lwp_mail_thread.o: $(TRACE_SRCDIR)/LWP/mail_thread.c
	$(CRULE_OPT)
trace_pthread_mail_thread.o: $(TRACE_SRCDIR)/PTHREAD/mail_thread.c
	$(CRULE_OPT)
trace_userspace_mail.o: $(TRACE_SRCDIR)/USERSPACE/mail.c
	$(CRULE_OPT)
trace_userspace_gen_rgy.o: $(TRACE_SRCDIR)/USERSPACE/gen_rgy.c
	$(CRULE_OPT)
trace_userspace_syscall.o: $(TRACE_SRCDIR)/USERSPACE/syscall.c
	$(CRULE_OPT)
trace_userspace_syscall_probe.o: $(TRACE_SRCDIR)/USERSPACE/syscall_probe.c
	$(CRULE_OPT)


# ugliness.  we pull in certain source files from other
# parts of the namespace.  eventually the code should
# be migrated into osi, or should be built into libs we
# can link against

comerr.o: $(COMERR_SRCDIR)/com_err.c
	$(CRULE_OPT)

comerr_et_name.o: $(COMERR_SRCDIR)/et_name.c
	$(CRULE_OPT)

comerr_error_msg.o: $(COMERR_SRCDIR)/error_msg.c
	$(CRULE_OPT)

lwp_shlock.o: $(LWP_SRCDIR)/lock.c
	$(CRULE_OPT)

util_casestrcpy.o: $(UTIL_SRCDIR)/casestrcpy.c
	$(CRULE_OPT)

