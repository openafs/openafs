/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef lint
#endif
/*
 * This module implemenets the logical Local Clock, as described in section
 * 5. of the NTP specification.
 *
 * Revision 2.2  90/09/19  16:23:58
 * Changed most AIX conditionals to AIX22 since, 3.1 has adjtime call.
 * On HP only call settimeofday if accumulated offset is at least 2 msec.
 * 
 * Revision 2.1  90/08/07  19:22:51
 * Start with clean version to sync test and dev trees.
 * 
 * Revision 1.9  90/05/24  16:20:57
 * For AIX add code to set the bias according to current offset begin
 *   positive or negative.
 * Fix bug where cum.tv_sec wasn't being added in.
 * Reorganize HPUX conditionals a bit.
 * 
 * Revision 1.8  90/05/21  13:49:48
 * Modify local adjtime procedure to try really hard to set the time by shaving
 *   the usec field of the clock using stime.
 * 
 * Revision 1.7  89/12/22  20:30:19
 * hp/ux specific (initial port to it); Added <afs/param.h> and special include files for HPUX and misc other changes such as handle sysV stuff (i.e. adjtime()) alike AIX
 * 
 * Revision 1.6  89/12/11  14:25:26
 * Added code to support AIX 2.2.1.
 * 
 * Revision 1.5  89/05/24  12:25:22
 * Latest May 18, Version 4.3 release from UMD.
 * 
 * Revision 3.4.1.4  89/05/18  18:23:36
 * A couple of changes to debug NeXT support in ntp_adjust.c
 * 
 * Revision 3.4.1.3  89/04/07  18:05:17
 * Removed unused variable from ntp_adjust.c module.
 * 
 * Revision 3.4.1.2  89/03/22  18:30:52
 * patch3: Use new RCS headers.
 * 
 * Revision 3.4.1.1  89/03/20  00:09:06
 * patch1: Don't zero the drift compensation or compliance values when a step
 * patch1: adjustment of the clock occurs.  Use symbolic definition of
 * patch1: CLOCK_FACTOR rather than constant.
 * 
 * Revision 3.4  89/03/17  18:37:03
 * Latest test release.
 * 
 * Revision 3.3.1.2  89/03/17  18:25:03
 * Applied suggested code from Dennis Ferguson for logical clock model based on
 * the equations in section 5.  Many thanks.
 * 
 * Revision 3.3.1.1  89/03/16  19:19:29
 * Attempt to implement using the equations in section 5 of the NTP spec, 
 * rather then modeling the Fuzzball implementation.
 * 
 * Revision 3.3  89/03/15  14:19:45
 * New baseline for next release.
 * 
 * Revision 3.2.1.1  89/03/15  13:47:24
 * Use "%f" in format strings rather than "%lf".
 * 
 * Revision 3.2  89/03/07  18:22:54
 * New version of UNIX NTP daemon and software based on the 6 March 1989
 * draft of the new NTP protocol specification.  This module attempts to
 * conform to the new logical clock described in section 5 of the spec.  Note
 * that the units of the drift_compensation register have changed.
 * 
 * This version also accumulates the residual adjtime() truncation and
 * adds it in on subsequent adjustments.
 * 
 * Revision 3.1.1.1  89/02/15  08:55:48
 * *** empty log message ***
 * 
 * 
 * Revision 3.1  89/01/30  14:43:08
 * Second UNIX NTP test release.
 * 
 * Revision 3.0  88/12/12  16:00:38
 * Test release of new UNIX NTP software.  This version should conform to the
 * revised NTP protocol specification.
 * 
 */

#include <afs/param.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/resource.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <strings.h>
#include <errno.h>
#if defined(AIX)
#include <sys/syslog.h>
#else
#include <syslog.h>
#endif /* defined(AIX) */

#include "ntp.h"

#ifdef	DEBUG
extern int debug;
#endif

extern int doset;
extern int debuglevel;
extern int kern_tickadj;
extern char *ntoa();
extern struct sysdata sys;

double	drift_comp = 0.0,
	compliance,
	clock_adjust;
afs_int32	update_timer = 0;

int	adj_precision;
double	adj_residual;
int	firstpass = 1;

#define MICROSECONDS 1000000

#define	abs(x)	((x) < 0 ? -(x) : (x))

void
init_logical_clock()
{
	if (kern_tickadj)
		adj_precision = kern_tickadj;
	else
		adj_precision = 1;
	/*
	 *  If you have the "fix" for adjtime() installed in you kernel, you'll
	 *  have to make sure that adj_precision is set to 1 here.
	 */
}


/*
 *  5.0 Logical clock procedure
 *
 *  Only paramter is an offset to vary the clock by, in seconds.  We'll either
 *  arrange for the clock to slew to accomodate the adjustment, or just preform
 *  a step adjustment if the offset is too large.
 *
 *  The update which is to be performed is left in the external
 *  clock_adjust. 
 *
 *  Returns non-zero if clock was reset rather than slewed.
 *
 *  Many thanks for Dennis Ferguson <dennis@gw.ccie.utoronto.ca> for his
 *  corrections to my code.
 */

int
adj_logical(offset)
	double offset;
{
	struct timeval tv1, tv2;
#ifdef	XADJTIME2
	struct timeval delta, olddelta;
#endif
	
	/*
	 *  Now adjust the logical clock
	 */
	if (!doset)
		return 0;

	adj_residual = 0.0;
	if (offset > CLOCK_MAX || offset < -CLOCK_MAX) {
		double steptime = offset;

		(void) gettimeofday(&tv2, (struct timezone *) 0);
		steptime += tv2.tv_sec;
		steptime += tv2.tv_usec / 1000000.0;
		tv1.tv_sec = steptime;
		tv1.tv_usec = (steptime - tv1.tv_sec) * 1000000;
#ifdef	DEBUG
		if (debug > 2) {
			steptime = (tv1.tv_sec + tv1.tv_usec/1000000.0) -
				(tv2.tv_sec + tv2.tv_usec/1000000.0);
			printf("adj_logical: %f %f\n", offset, steptime);
		}
#endif
		if (settimeofday(&tv1, (struct timezone *) 0) < 0)
		    {
			syslog(LOG_ERR, "Can't set time: %m");
			return(-1);
		    }
		firstpass = 1;
		update_timer = 0;
		clock_adjust = 0.0;
		return (1);	  /* indicate that step adjustment was done */
	} else 	{
		double ai;

		/*
		 * If this is our very first adjustment, don't touch
		 * the drift compensation (this is f in the spec
		 * equations), else update using the *old* value
		 * of the compliance.
		 */
		clock_adjust = offset;
		if (firstpass)
			firstpass = 0;
		else if (update_timer > 0) {
			ai = abs(compliance);
			ai = (double)(1<<CLOCK_COMP) - 
				(double)(1<<CLOCK_FACTOR) * ai;
			if (ai < 1.0)		/* max(... , 1.0) */
				ai = 1.0;
			drift_comp += offset / (ai * (double)update_timer);
		}

		/*
		 * Set the timer to zero.  adj_host_clock() increments it
		 * so we can tell the period between updates.
		 */
		update_timer = 0;

		/*
		 * Now update the compliance.  The compliance is h in the
		 * equations.
		 */
		compliance += (offset - compliance)/(double)(1<<CLOCK_TRACK);

#ifdef XADJTIME2
		delta.tv_sec = offset;
		delta.tv_usec = (offset - delta.tv_sec) * 1000;
		(void) adjtime2(&delta, &olddelta);
#endif
		return(0);
	}
}

#ifndef	XADJTIME2
extern int adjtime();

/*
 *  This is that routine that performs the periodic clock adjustment.
 *  The procedure is best described in the the NTP document.  In a
 *  nutshell, we prefer to do lots of small evenly spaced adjustments.
 *  The alternative, one large adjustment, creates two much of a
 *  clock disruption and as a result oscillation.
 *
 *  This function is called every 2**CLOCK_ADJ seconds.
 *
 */

/*
 * global for debugging?
 */
double adjustment;

void
adj_host_clock()
{

	struct timeval delta, olddelta;

	if (!doset)
		return;

	/*
	 * Add update period into timer so we know how long it
	 * took between the last update and the next one.
	 */
	update_timer += 1<<CLOCK_ADJ;
	/*
	 * Should check to see if update_timer > 1 day here?
	 */

	/*
	 * Compute phase part of adjustment here and update clock_adjust.
	 * Note that the equations used here are implicit in the last
	 * two equations in the spec (in particular, look at the equation
	 * for g and figure out how to  find the k==1 term given the k==0 term.)
	 */
	adjustment = clock_adjust / (double)(1<<CLOCK_PHASE);
	clock_adjust -= adjustment;

	/*
	 * Now add in the frequency component.  Be careful to note that
	 * the ni occurs in the last equation since those equations take
	 * you from 64 second update to 64 second update (ei is the total
	 * adjustment done over 64 seconds) and we're only deal in the
	 * little 4 second adjustment interval here.
	 */
	adjustment += drift_comp / (double)(1<<CLOCK_FREQ);

	/*
	 * Add in old adjustment residual
	 */
	adjustment += adj_residual;

	/*
	 * Simplify.  Adjustment shouldn't be bigger than 2 ms.  Hope
	 * writer of spec was truth telling.
	 */
#ifdef	DEBUG
	delta.tv_sec = adjustment;
	if (debug && delta.tv_sec) abort();
#else
	delta.tv_sec = 0;
#endif
#if defined(AFS_AIX32_ENV)
	/* aix 3.1 ajdtime has unsigned bug. */
	if (adjustment < 0.0) adj_precision = 5000;
	else if (adjustment < 1000000.0) adj_precision = 1000;
	else adj_precision = 5000;
#endif
	delta.tv_usec = ((afs_int32)(adjustment * 1000000.0) / adj_precision)
		   * adj_precision;

	adj_residual = adjustment - (double) delta.tv_usec / 1000000.0;

	if (delta.tv_usec == 0)
		return;

	if (adjtime(&delta, &olddelta) < 0)
		syslog(LOG_ERR, "Can't adjust time: %m");

#ifdef	DEBUG
	if(debug > 2)
		printf("adj: %ld us  %f %f\n",
		       delta.tv_usec, drift_comp, clock_adjust);
#endif
}
#endif

#if defined(AFS_HPUX_ENV) && !defined(AFS_HPUX102_ENV)
/*
 * adjtime for HPUX.  Basically a poor man's version of the BSD adjtime call.
 * It accumulates adjustments until such time as there are enough to use the
 * stime primitive.  This code is ugly.  It also does no range checking,
 * since the assumption is that the adjustments will be (fairly) small.
 *
 * The basic mechanism is simple: we only have a coarse granularity (1
 * second)  so we accumulate changes until they exceed 1 second.  When
 * that is the case, an adjustment is made which will always be less
 * than 1 second.  If it is a negative adjustment, we won't allow it
 * to timewarp more frequently than once every ten seconds.
 * 
 */
static struct timeval cum = {0,0};	/* use to accumulate unadjusted time */

ZeroAIXcum()
{
    if (debug > 6)
	printf ("Zeroing aix_adjtime accumulation: %d %d\n",
		cum.tv_sec, cum.tv_usec);
    bzero (&cum, sizeof(cum));
}

int adjtime(newdelta, olddelta)
   struct timeval *newdelta;
   struct timeval *olddelta;
{
    static struct timeval lastbackadj; /* last time adjusted backwards */
    struct timeval now, new;

#if 0
    {	/* punt accumulated change if sign of correction changes */
	int dNeg = ((newdelta->tv_sec < 0 || newdelta->tv_usec < 0) ? 1 : 0);
	int cNeg = ((cum.tv_sec < 0 || cum.tv_usec < 0) ? 1 : 0);
	if (dNeg != cNeg) ZeroAIXcum();
    }
#endif /* 0 */

    *olddelta = cum;
    cum.tv_usec += newdelta->tv_usec;
    cum.tv_sec += newdelta->tv_sec + cum.tv_usec / MICROSECONDS;
    cum.tv_usec = (cum.tv_usec < 0) ? -((-cum.tv_usec) % MICROSECONDS) :
	cum.tv_usec % MICROSECONDS;

    gettimeofday (&now, (struct timezone *)0);
    new = now;
    new.tv_sec += cum.tv_sec;
    new.tv_usec += cum.tv_usec;
    if (new.tv_usec >= MICROSECONDS) new.tv_sec++, new.tv_usec -= MICROSECONDS;
    else if (new.tv_usec < 0) new.tv_sec--, new.tv_usec += MICROSECONDS;
    if (debug > 6) {
	printf ("cum is %d %d, new is %d %d, now is %d %d\n",
		cum.tv_sec, cum.tv_usec,
		new.tv_sec, new.tv_usec, now.tv_sec, now.tv_usec);
    }

    if (cum.tv_sec || abs(cum.tv_usec) > 2000) {
	/* wait till accumulated update is at least 2msec since this call
         * seems to add some jitter. */
	settimeofday (&new, (struct timezone *)0);
	if (debug > 4)			/* do set before doing I/O */
	    printf ("hp_adjtime: pushing clock by %d usec w/ settimeofday \n",
		    cum.tv_sec*1000000 + cum.tv_usec);
	cum.tv_sec = 0;
	cum.tv_usec = 0;
    }
    return 0;
}
#endif /* defined(AFS_HPUX_ENV) */
