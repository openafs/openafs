/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*******************************************************************\
* 								    *
* 	Information Technology Center				    *
* 	Carnegie-Mellon University				    *
* 								    *
\*******************************************************************/
#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header: /cvs/openafs/src/lwp/preempt.c,v 1.16.2.1 2005/07/11 18:59:55 shadow Exp $");


#include "lwp.h"
#include "preempt.h"

#if defined(AFS_LINUX20_ENV) || defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV) || defined(AFS_DJGPP_ENV)
int PRE_Block = 0;


int
PRE_InitPreempt(struct timeval *slice)
{
    return LWP_SUCCESS;
}

int
PRE_EndPreempt()
{
    return LWP_SUCCESS;
}

#else
#include <sys/time.h>
#include <signal.h>
#ifdef HAVE_UCONTEXT_H
#include <ucontext.h>
#endif

#if defined(AFS_OSF_ENV) || defined(AFS_S390_LINUX20_ENV)
int PRE_Block = 0;		/* used in lwp.c and process.s */
#else
char PRE_Block = 0;		/* used in lwp.c and process.s */
#endif

#if HAVE_SIGACTION && defined(SA_SIGINFO)
static void
AlarmHandler(int sig, siginfo_t *st, ucontext_t *scp)
#else
static void
AlarmHandler(int sig, int code, struct sigcontext *scp)
#endif
{
    if (PRE_Block == 0 && lwp_cpptr->level == 0) {
	PRE_BeginCritical();
#if HAVE_SIGACTION && defined(SA_SIGINFO)
	sigprocmask(SIG_SETMASK, &scp->uc_sigmask, NULL);
#else
	sigsetmask(scp->sc_mask);
#endif
	LWP_DispatchProcess();
	PRE_EndCritical();
    }

}

int
PRE_InitPreempt(struct timeval *slice)
{
    struct itimerval itv;
#if HAVE_SIGACTION && defined(SA_SIGINFO)
    struct sigaction action;
#else
    struct sigvec vec;
#endif

    if (lwp_cpptr == 0)
	return (LWP_EINIT);

    if (slice == 0) {
	itv.it_interval.tv_sec = itv.it_value.tv_sec = DEFAULTSLICE;
	itv.it_interval.tv_usec = itv.it_value.tv_usec = 0;
    } else {
	itv.it_interval = itv.it_value = *slice;
    }

#if HAVE_SIGACTION && defined(SA_SIGINFO)
    memset((char *)&action, 0, sizeof(action));
    action.sa_sigaction = AlarmHandler;
    action.sa_flags = SA_SIGINFO;

    if ((sigaction(SIGALRM, &action, NULL) == -1)
	|| (setitimer(ITIMER_REAL, &itv, NULL) == -1))
	return (LWP_ESYSTEM);
#else
    memset((char *)&vec, 0, sizeof(vec));
    vec.sv_handler = AlarmHandler;
    vec.sv_mask = vec.sv_onstack = 0;

    if ((sigvec(SIGALRM, &vec, (struct sigvec *)0) == -1)
	|| (setitimer(ITIMER_REAL, &itv, (struct itimerval *)0) == -1))
	return (LWP_ESYSTEM);
#endif

    return (LWP_SUCCESS);
}

int
PRE_EndPreempt()
{
    struct itimerval itv;
#if HAVE_SIGACTION && defined(SA_SIGINFO)
    struct sigaction action;
#else
    struct sigvec vec;
#endif

    if (lwp_cpptr == 0)
	return (LWP_EINIT);

    itv.it_value.tv_sec = itv.it_value.tv_usec = 0;

#if HAVE_SIGACTION && defined(SA_SIGINFO)
    memset((char *)&action, 0, sizeof(action));
    action.sa_handler = SIG_DFL;

    if ((setitimer(ITIMER_REAL, &itv, NULL) == -1)
	|| (sigaction(SIGALRM, &action, NULL) == -1))
	return (LWP_ESYSTEM);

#else
    memset((char *)&vec, 0, sizeof(vec));
    vec.sv_handler = SIG_DFL;
    vec.sv_mask = vec.sv_onstack = 0;

    if ((setitimer(ITIMER_REAL, &itv, (struct itimerval *)0) == -1)
	|| (sigvec(SIGALRM, &vec, (struct sigvec *)0) == -1))
	return (LWP_ESYSTEM);
#endif
    return (LWP_SUCCESS);
}

#endif /* AFS_LINUX20_ENV */
