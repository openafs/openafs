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
LIBTRACE_DEPS_libosi_trace_analyzer_shlib_pthread = \
	${MT_LIBS} ${OSI_LIBS} -L$(TOP_OBJDIR)/lib \
	-losi_trace_consumer-$(LIBTRACE_BUILDING_THR)-$(LIBTRACE_BUILDING_BIT) \
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

LIBTRACE_CFLAGS_libosi_trace_analyzer = ${COMMON_INCLUDE} ${CFLAGS} ${OSI_CFLAGS} ${ARCHFLAGS} ${XCFLAGS} -DOSI_TRACEPOINT_DISABLE
LIBTRACE_OBJS_libosi_trace_analyzer = \
	counter.o \
	init.o \
	logic.o \
	sum.o \
	var.o \
	$(NULL)


# userspace threading model-specific build objects

LIBTRACE_CFLAGS_userspace_lwp = 
LIBTRACE_OBJS_userspace_lwp = \
	$(NULL)

LIBTRACE_CFLAGS_ukernel_lwp = 
LIBTRACE_OBJS_ukernel_lwp = \
	$(NULL)

LIBTRACE_CFLAGS_libosi_trace_analyzer_lwp = 
LIBTRACE_OBJS_libosi_trace_analyzer_lwp = \
	$(NULL)

LIBTRACE_CFLAGS_userspace_pthread = ${MT_CFLAGS}
LIBTRACE_OBJS_userspace_pthread = \
	$(NULL)

LIBTRACE_CFLAGS_ukernel_pthread = 
LIBTRACE_OBJS_ukernel_pthread = \
	$(NULL)

LIBTRACE_CFLAGS_libosi_trace_analyzer_pthread = 
LIBTRACE_OBJS_libosi_trace_analyzer_pthread = \
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

LIBTRACE_CFLAGS_libosi_trace_analyzer_32 = 
LIBTRACE_OBJS_libosi_trace_analyzer_32 = \
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

LIBTRACE_CFLAGS_libosi_trace_analyzer_64 = 
LIBTRACE_OBJS_libosi_trace_analyzer_64 = \
	$(NULL)

# archive versus shlib stuff

LIBTRACE_CFLAGS_libosi_trace_analyzer_archive =
LIBTRACE_OBJS_libosi_trace_analyzer_archive = \
	$(NULL)

LIBTRACE_CFLAGS_libosi_trace_analyzer_shlib = ${SHLIB_CFLAGS}
LIBTRACE_OBJS_libosi_trace_analyzer_shlib = \
	$(NULL)


# build rules for OS Interface library

counter.o: $(TRACE_SRCDIR)/analyzer/counter.c
	$(CRULE_OPT)

init.o: $(TRACE_SRCDIR)/analyzer/init.c
	$(CRULE_OPT)

logic.o: $(TRACE_SRCDIR)/analyzer/logic.c
	$(CRULE_OPT)

sum.o: $(TRACE_SRCDIR)/analyzer/sum.c
	$(CRULE_OPT)

var.o: $(TRACE_SRCDIR)/analyzer/var.c
	$(CRULE_OPT)
