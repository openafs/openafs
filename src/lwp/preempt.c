
#ifndef lint
#endif

/*
****************************************************************************
*        Copyright IBM Corporation 1988, 1989 - All Rights Reserved        *
*                                                                          *
* Permission to use, copy, modify, and distribute this software and its    *
* documentation for any purpose and without fee is hereby granted,         *
* provided that the above copyright notice appear in all copies and        *
* that both that copyright notice and this permission notice appear in     *
* supporting documentation, and that the name of IBM not be used in        *
* advertising or publicity pertaining to distribution of the software      *
* without specific, written prior permission.                              *
*                                                                          *
* IBM DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL *
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL IBM *
* BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY      *
* DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER  *
* IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING   *
* OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.    *
****************************************************************************
*/

/*******************************************************************\
* 								    *
* 	Information Technology Center				    *
* 	Carnegie-Mellon University				    *
* 								    *
\*******************************************************************/
#include <afs/param.h>

#ifdef AFS_LINUX20_ENV
int PRE_Block = 0;
#else
#include <sys/time.h>
#include <signal.h>
#include <ucontext.h>
#include "lwp.h"
#include "preempt.h"

#ifdef	AFS_OSF_ENV
int PRE_Block = 0;		/* used in lwp.c and process.s */
#else
char PRE_Block = 0;		/* used in lwp.c and process.s */
#endif

static void AlarmHandler(sig, st, scp)
    int sig;
    siginfo_t *st;
    ucontext_t *scp;
    {
    if (PRE_Block == 0 && lwp_cpptr->level == 0)
	{
	PRE_BeginCritical();
	sigprocmask(SIG_SETMASK, &scp->uc_sigmask, NULL);
	LWP_DispatchProcess();
	PRE_EndCritical();
	}
    
    }

int PRE_InitPreempt(slice)
    struct timeval *slice;
    {
    struct itimerval itv;
    struct sigaction action;

    if (lwp_cpptr == 0) return (LWP_EINIT);
    
    if (slice == 0)
	{
	itv.it_interval.tv_sec = itv.it_value.tv_sec = DEFAULTSLICE;
	itv.it_interval.tv_usec = itv.it_value.tv_usec = 0;
	}
    else
	{
	itv.it_interval = itv.it_value = *slice;
	}

    bzero((char *)&action, sizeof(action));
    action.sa_sigaction = AlarmHandler;
    action.sa_flags = SA_SIGINFO;

    if ((sigaction(SIGALRM, &action, (struct sigaction *)0) == -1) ||
	(setitimer(ITIMER_REAL, &itv, (struct itimerval *) 0) == -1))
	return(LWP_ESYSTEM);

    return(LWP_SUCCESS);
    }

int PRE_EndPreempt()
    {
    struct itimerval itv;
    struct sigaction action;

    if (lwp_cpptr == 0) return (LWP_EINIT);
    
    itv.it_value.tv_sec = itv.it_value.tv_usec = 0;

    bzero((char *)&action, sizeof(action));
    action.sa_handler = SIG_DFL;

    if ((setitimer(ITIMER_REAL, &itv, (struct itimerval *) 0) == -1) ||
	(sigaction(SIGALRM, &action, (struct sigaction *)0) == -1))
	return(LWP_ESYSTEM);

    return(LWP_SUCCESS);
    }

#endif /* AFS_I386_LINUX20_ENV */
