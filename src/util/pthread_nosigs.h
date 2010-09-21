/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef AFS_PTHREAD_NOSIGS_H
#define AFS_PTHREAD_NOSIGS_H

/* We want servers to only handle signals in the main thread. To do this
 * we must block signals in the child threads. To do this, wrap pthread_create
 * with these macros.
 */

#ifdef AFS_NT40_ENV
/* Compilers complain about empty declaration unless at end of decl's */
#define AFS_SIGSET_DECL int i_junk=0
#define AFS_SIGSET_CLEAR() ++i_junk
#define AFS_SIGSET_RESTORE() --i_junk
#else
#define AFS_SIGSET_DECL sigset_t i_tset, i_oset
#ifdef AFS_AIX42_ENV
#define AFS_SET_SIGMASK sigthreadmask
#else
#define AFS_SET_SIGMASK pthread_sigmask
#endif
#ifdef SIGSEGV
#define _SETSEGV sigdelset(&i_tset, SIGSEGV);
#else
#define _SETSEGV ;
#endif
#ifdef SIGBUS
#define _SETBUS sigdelset(&i_tset, SIGBUS);
#else
#define _SETBUS ;
#endif
#ifdef SIGILL
#define _SETILL sigdelset(&i_tset, SIGILL);
#else
#define _SETILL ;
#endif
#ifdef SIGTRAP
#define _SETTRAP sigdelset(&i_tset, SIGTRAP);
#else
#define _SETTRAP ;
#endif
#ifdef SIGABRT
#define _SETABRT sigdelset(&i_tset, SIGABRT);
#else
#define _SETABRT ;
#endif
#ifdef SIGFPE
#define _SETFPE sigdelset(&i_tset, SIGFPE);
#else
#define _SETFPE ;
#endif
#define AFS_SIGSET_CLEAR() \
do { \
	 sigfillset(&i_tset); \
         _SETSEGV \
         _SETBUS \
         _SETILL \
         _SETTRAP \
         _SETABRT \
         _SETFPE \
	 assert(AFS_SET_SIGMASK(SIG_BLOCK, &i_tset, &i_oset) == 0); \
} while (0)

#define AFS_SIGSET_RESTORE() \
do { \
	 assert(AFS_SET_SIGMASK(SIG_SETMASK, &i_oset, NULL) == 0); \
} while (0)
#endif /* AFS_NT40_ENV */

#endif /* AFS_PTHREAD_NOSIGS_H */
