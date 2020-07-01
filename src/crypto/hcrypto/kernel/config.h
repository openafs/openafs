/*
 * Copyright (c) 2010 Your File System Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR `AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <afsconfig.h>
#include "afs/param.h"
#include "afs/opr.h"

#include "afs/stds.h"
#include "afs/sysincludes.h"
#include "afs/afsincludes.h"
#include "afs/afs_prototypes.h"

#if defined(assert)
#undef assert
#endif
#define assert osi_Assert

/* Linux's current.h defines current to get_current(), conflicing with
 * heimdal's rand-fortuna.c's local variable. */
#if defined(current)
#undef current
#define current current
#endif

/* AIX and some Solaris (and others, presumably) still have a 'u' symbol for
 * the user area.  rand-fortuna.c has a local variable of that name. */
#if defined(u)
#undef u
#define u u
#endif

/* hcrypto uses "static inline", which isn't supported by some of our
 * compilers */
#if !defined(inline) && !defined(__GNUC__)
#define inline
#endif

/* We need wrappers for the various memory management functions */
#define calloc _afscrypto_calloc
void * _afscrypto_calloc(int, size_t);

#define malloc _afscrypto_malloc
void * _afscrypto_malloc(size_t);

#define free _afscrypto_free
void _afscrypto_free(void *);

#define strdup _afscrypto_strdup
char * _afscrypto_strdup(const char *);

#define realloc _afscrypto_realloc
void * _afscrypto_realloc(void *, size_t);

/* we may not have strcasecmp in the kernel */
#define strcasecmp afs_strcasecmp

/* osi_readRandom is also prototyped in afs_prototypes.h, but pulling that in
 * here creates loads of additional dependencies */
extern int osi_readRandom(void *, afs_size_t);

#if defined(getpid)
/* On linux, getpid() is unfortunately declared in terms of current, which
 * already gives us a namespace clash.  It was lousy entropy, anyway. */
#undef getpid
#define getpid()	1
#else
static_inline pid_t getpid(void) {return 1;};
#endif
static_inline int open(const char *path, int flags, ...) {return -1;}
static_inline void abort(void) {osi_Panic("hckernel aborting\n");}
static_inline void rk_cloexec(int fd) {}
static_inline ssize_t read(int d, void *buf, size_t nbytes) {return -1;}
static_inline int close(int d) {return -1;}
#if defined(HAVE_GETUID)
#undef HAVE_GETUID
#endif
#ifdef HAVE_ARC4RANDOM
# undef HAVE_ARC4RANDOM
#endif

#if !defined(AFS_LINUX26_ENV)
/*
 * gettimeofday is only used in rand-fortuna.c, not built for Linux.
 * Linux 5.6 removes the native struct timeval, so this stub would not build.
 */
static_inline int gettimeofday(struct timeval *tp, void *tzp)
    {if (tp == NULL) return -1; tp->tv_sec = osi_Time(); tp->tv_usec = 0; return 0;}
#endif

#if defined(KERNEL) && (defined(AFS_SUN5_ENV) || defined(AFS_ARM64_LINUX26_ENV))
/*
 * Some functions such as RAND_add take a 'double' as an argument, but floating
 * point code generally cannot be used in kernelspace. We never actually use
 * that argument in kernel code, but just that it exists as an argument is
 * enough to break the kernel code on Linux (on arm64) and Solaris (depending
 * on the compiler version and flags). Change all instances of double to void*
 * to avoid this; if someone does try to use that argument, hopefully the fact
 * that it is now a void* will flag an error at compile time before it causes
 * any further problems.
 */
# define double void*
#endif
