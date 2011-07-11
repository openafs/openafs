/*
 * This file, written by Garrett Wollman <wollman@bimajority.org>, is
 * in the public domain.
 */
#include <afsconfig.h>
#include <afs/param.h>

#include "afsutil.h"

#if AFS_PTHREAD_ENV && !defined(AFS_NT40_ENV)
# include <pthread.h>
# ifdef HAVE_PTHREAD_NP_H
#  include <pthread_np.h>
# endif

void
afs_pthread_setname(pthread_t thread, const char *threadname)
{
# if defined(HAVE_PTHREAD_SET_NAME_NP)
	/* FreeBSD style */
	pthread_set_name_np(thread, threadname);
# elif defined(HAVE_PTHREAD_SETNAME_NP)
#  if PTHREAD_SETNAME_NP_ARGS == 3
	/* DECthreads style */
	pthread_setname_np(thread, threadname, (void *)0);
#  elif PTHREAD_SETNAME_NP_ARGS == 2
	/* GNU libc on Linux style */
	pthread_setname_np(thread, threadname);
#  elif PTHREAD_SETNAME_NP_ARGS == 1
	/* Mac OS style */
	if (thread == pthread_self())
		pthread_setname_np(threadname);
#  else
#    error "Could not identify your pthread_setname_np() implementation"
#  endif
# endif
}

void
afs_pthread_setname_self(const char *threadname)
{
	afs_pthread_setname(pthread_self(), threadname);
}
#endif
