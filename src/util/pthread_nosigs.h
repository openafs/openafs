/* Copyright (C)  1999  IBM Corporation.  All rights reserved. */

#ifndef AFS_PTHREAD_NOSIGS_H
#define AFS_PTHREAD_NOSIGS_H

/* We want servers to only handle signals in the main thread. To do this
 * we must block signals in the child threads. To do this, wrap pthread_create
 * with these macros.
 */

#ifdef AFS_NT40_ENV
/* Compilers complain about empty declaration unless at end of decl's */
#define AFS_SIGSET_DECL int i_junk
#define AFS_SIGSET_CLEAR()
#define AFS_SIGSET_RESTORE()
#else
#define AFS_SIGSET_DECL sigset_t i_tset, i_oset
#ifdef AFS_AIX42_ENV
#define AFS_SET_SIGMASK sigthreadmask
#else
#define AFS_SET_SIGMASK pthread_sigmask
#endif
#define AFS_SIGSET_CLEAR() \
do { \
	 sigfillset(&i_tset); \
	 assert(AFS_SET_SIGMASK(SIG_BLOCK, &i_tset, &i_oset) == 0); \
} while (0)

#define AFS_SIGSET_RESTORE() \
do { \
	 assert(AFS_SET_SIGMASK(SIG_SETMASK, &i_oset, NULL) == 0); \
} while (0)
#endif /* AFS_NT40_ENV */

#endif /* AFS_PTHREAD_NOSIGS_H */
