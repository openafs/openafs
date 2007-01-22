# Copyright 2006, Sine Nomine Associates and others.
# All Rights Reserved.
# 
# This software has been released under the terms of the IBM Public
# License.  For details, see the LICENSE file in the top-level source
# directory or online at http://www.openafs.org/dl/license10.html


# Solaris specific libosi object files:

LIBOSI_OS_CFLAGS_COMMON = 
LIBOSI_OS_OBJS_COMMON = \
	$(NULL)

LIBOSI_OS_CFLAGS_kernelspace = 
LIBOSI_OS_OBJS_kernelspace = \
	osi_common_mem_local.o \
	osi_legacy_mem_align.o \
	osi_legacy_kshlock.o \
	osi_native_kcache.o \
	osi_native_kcpu.o \
	osi_native_kmem.o \
	osi_native_kproc.o \
	osi_native_kthread.o \
	$(NULL)

LIBOSI_OS_CFLAGS_kernel = 
LIBOSI_OS_OBJS_kernel = \
	$(NULL)

LIBOSI_OS_CFLAGS_userspace = 
LIBOSI_OS_OBJS_userspace = \
	osi_native_ucache.o \
	osi_native_ucpu.o \
	osi_native_utime_approx.o \
	osi_native_utime_approx_inline.o \
	osi_native_utime_inline.o \
	$(NULL)

LIBOSI_OS_CFLAGS_ukernel = 
LIBOSI_OS_OBJS_ukernel = \
	$(NULL)

LIBOSI_OS_CFLAGS_libosi = 
LIBOSI_OS_OBJS_libosi = \
	$(NULL)


LIBOSI_OS_CFLAGS_userspace_lwp =
LIBOSI_OS_OBJS_userspace_lwp = \
	$(NULL)

LIBOSI_OS_CFLAGS_ukernel_lwp =
LIBOSI_OS_OBJS_ukernel_lwp = \
	$(NULL)

LIBOSI_OS_CFLAGS_libosi_lwp =
LIBOSI_OS_OBJS_libosi_lwp = \
	$(NULL)


LIBOSI_OS_CFLAGS_userspace_pthread =
LIBOSI_OS_OBJS_userspace_pthread = \
	$(NULL)

LIBOSI_OS_CFLAGS_ukernel_pthread =
LIBOSI_OS_OBJS_ukernel_pthread = \
	$(NULL)

LIBOSI_OS_CFLAGS_libosi_pthread =
LIBOSI_OS_OBJS_libosi_pthread = \
	$(NULL)


LIBOSI_OS_CFLAGS_COMMON_32 = 
LIBOSI_OS_OBJS_COMMON_32 = \
	$(NULL)

LIBOSI_OS_CFLAGS_kernelspace_32 = 
LIBOSI_OS_OBJS_kernelspace_32 = \
	$(NULL)

LIBOSI_OS_CFLAGS_kernel_32 = 
LIBOSI_OS_OBJS_kernel_32 = \
	$(NULL)

LIBOSI_OS_CFLAGS_userspace_32 = 
LIBOSI_OS_OBJS_userspace_32 = \
	$(NULL)

LIBOSI_OS_CFLAGS_ukernel_32 = 
LIBOSI_OS_OBJS_ukernel_32 = \
	$(NULL)

LIBOSI_OS_CFLAGS_libosi_32 = 
LIBOSI_OS_OBJS_libosi_32 = \
	$(NULL)


LIBOSI_OS_CFLAGS_COMMON_64 = 
LIBOSI_OS_OBJS_COMMON_64 = \
	$(NULL)

LIBOSI_OS_CFLAGS_kernelspace_64 = 
LIBOSI_OS_OBJS_kernelspace_64 = \
	$(NULL)

LIBOSI_OS_CFLAGS_kernel_64 = 
LIBOSI_OS_OBJS_kernel_64 = \
	$(NULL)

LIBOSI_OS_CFLAGS_userspace_64 = 
LIBOSI_OS_OBJS_userspace_64 = \
	$(NULL)

LIBOSI_OS_CFLAGS_ukernel_64 = 
LIBOSI_OS_OBJS_ukernel_64 = \
	$(NULL)

LIBOSI_OS_CFLAGS_libosi_64 = 
LIBOSI_OS_OBJS_libosi_64 = \
	$(NULL)


LIBOSI_OS_CFLAGS_COMMON_inst = 
LIBOSI_OS_OBJS_COMMON_inst = \
	$(NULL)

LIBOSI_OS_CFLAGS_kernelspace_inst = 
LIBOSI_OS_OBJS_kernelspace_inst = \
	$(NULL)

LIBOSI_OS_CFLAGS_kernel_inst = 
LIBOSI_OS_OBJS_kernel_inst = \
	$(NULL)

LIBOSI_OS_CFLAGS_userspace_inst = 
LIBOSI_OS_OBJS_userspace_inst = \
	$(NULL)

LIBOSI_OS_CFLAGS_ukernel_inst = 
LIBOSI_OS_OBJS_ukernel_inst = \
	$(NULL)

LIBOSI_OS_CFLAGS_libosi_inst = 
LIBOSI_OS_OBJS_libosi_inst = \
	$(NULL)


LIBOSI_OS_CFLAGS_COMMON_ni = 
LIBOSI_OS_OBJS_COMMON_ni = \
	$(NULL)

LIBOSI_OS_CFLAGS_kernelspace_ni = 
LIBOSI_OS_OBJS_kernelspace_ni = \
	$(NULL)

LIBOSI_OS_CFLAGS_kernel_ni = 
LIBOSI_OS_OBJS_kernel_ni = \
	$(NULL)

LIBOSI_OS_CFLAGS_userspace_ni = 
LIBOSI_OS_OBJS_userspace_ni = \
	$(NULL)

LIBOSI_OS_CFLAGS_ukernel_ni = 
LIBOSI_OS_OBJS_ukernel_ni = \
	$(NULL)

LIBOSI_OS_CFLAGS_libosi_ni = 
LIBOSI_OS_OBJS_libosi_ni = \
	$(NULL)



LIBOSI_OS_CFLAGS_libosi_archive =
LIBOSI_OS_OBJS_libosi_archive = \
	$(NULL)

LIBOSI_OS_CFLAGS_libosi_shlib =
LIBOSI_OS_OBJS_libosi_shlib = \
	$(NULL)

