/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef	lint
#endif /* lint */
/*
 * Revision 2.1  90/08/07  19:23:23
 * Start with clean version to sync test and dev trees.
 * 
 * Revision 1.7  89/12/22  20:33:21
 * hp/ux specific (initial port to it); Added <afs/param.h> and special include files for HPUX and misc other changes
 * 
 * Revision 1.6  89/12/11  14:26:16
 * Added code to support AIX 2.2.1.
 * 
 * Revision 1.5  89/05/24  12:27:44
 * Latest May 18, Version 4.3 release from UMD.
 * 
 * Revision 3.4.1.3  89/05/18  18:33:50
 * Added support few a new type of unsigned long to double compiler brokenness,
 * called GENERIC_UNS_BUG.  If this is defined, then the unsigned long is
 * shifted right one bit, the high-order bit of the result is cleared, then
 * converted to a double.  The double is multiplied by 2.0, and the a 1.0 is
 * optionall added to it if the low order bit of the original unsigned long
 * was set.  Whew!
 * 
 * Revision 3.4.1.2  89/03/29  12:46:02
 * Check for success sending query before trying to listen for answers.  Will 
 * catch case of no server running and an ICMP port unreachable being returned.
 * 
 * Revision 3.4.1.1  89/03/22  18:32:19
 * patch3: Use new RCS headers.
 * 
 * Revision 3.4  89/03/17  18:37:18
 * Latest test release.
 * 
 * Revision 3.3  89/03/15  14:20:03
 * New baseline for next release.
 * 
 * Revision 3.2  89/03/07  18:29:22
 * New version of UNIX NTP daemon based on the 6 March 1989 draft of the new
 * NTP protocol spec.  This module has mostly cosmetic changes.
 * 
 * Revision 3.1.1.1  89/02/15  08:59:27
 * *** empty log message ***
 * 
 * 
 * Revision 3.1  89/01/30  14:43:18
 * Second UNIX NTP test release.
 * 
 * Revision 3.0  88/12/12  15:58:59
 * Test release of new UNIX NTP software.  This version should conform to the
 * revised NTP protocol specification.
 * 
 */

#include <afs/param.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#if defined(AIX)
#include <sys/syslog.h>
#else
#include <syslog.h>
#endif
#include "ntp.h"

extern int errno;

#define	TRUE	1
#define	FALSE	0

/*
 *  The nice thing here is that the quantity is NEVER signed.
 */
double
ul_fixed_to_double(t)
	struct l_fixedpt *t;
{
	double a, b;
#ifdef	GENERIC_UNS_BUG
	register int i;

	i = ntohl(t->fraction);
	a = (afs_int32)((i >> 1) & 0x7fffffff);
	a *= 2.0;
	if (i & 1)
		a += 1.0;
	a = a / (4.294967296e9);	/* shift dec point over by 32 bits */
	i = ntohl(t->int_part);
	b = (afs_int32)((i >> 1) & 0x7fffffff);
	b *= 2.0;
	if (i & 1)
		b += 1.0;
#else	/* GENERIC_UNS_BUG */
	a = (afs_uint32) ntohl(t->fraction);
#ifdef	VAX_COMPILER_FLT_BUG
	if (a < 0.0) a += 4.294967296e9;
#endif
	a = a / (4.294967296e9);/* shift dec point over by 32 bits */
	b = (afs_uint32) ntohl(t->int_part);
#ifdef	VAX_COMPILER_FLT_BUG
	if (b < 0.0) b += 4.294967296e9;
#endif
#endif	/* GENERIC_UNS_BUG */
	return (a + b);
}

/*
 *  Here we have to worry about the high order bit being signed
 */

#if	0
double
l_fixed_to_double(t)
	struct l_fixedpt *t;
{
	double a,b;

	if (ntohl(t->int_part) & 0x80000000) {
		a = ntohl(~t->fraction);
#ifdef	VAX_COMPILER_FLT_BUG
		if (a < 0.0) a += 4.294967296e9;
#endif
		a = a / (4.294967296e9);
		b = ntohl(~t->int_part);
#ifdef	VAX_COMPILER_FLT_BUG
		if (b < 0.0) b += 4.294967296e9;
#endif
		a += b;
		a = -a;
	} else {
		a = ntohl(t->fraction);
#ifdef	VAX_COMPILER_FLT_BUG
		if (a < 0.0) a += 4.294967296e9;
#endif
		a = a / (4.294967296e9);
		b = ntohl(t->int_part);
#ifdef	VAX_COMPILER_FLT_BUG
		if (b < 0.0) b += 4.294967296e9;
#endif
		a += b;
	}
	return (a);
}
#endif

/*
 *  Here we have to worry about the high order bit being signed
 */
double
s_fixed_to_double(t)
	struct s_fixedpt *t;
{
	double a;

	if (ntohs(t->int_part) & 0x8000) {
		a = ntohs(~t->fraction & 0xFFFF);
		a = a / 65536.0;	/* shift dec point over by 16 bits */
		a +=  ntohs(~t->int_part & 0xFFFF);
		a = -a;
	} else {
		a = ntohs(t->fraction);
		a = a / 65536.0;	/* shift dec point over by 16 bits */
		a += ntohs(t->int_part);
	}
	return (a);
}

void
double_to_l_fixed(t, value)
	struct l_fixedpt *t;
	double value;
{
	double temp;

	if (value >= (double) 0.0) {
		t->int_part = value;
		temp = value - t->int_part;
		temp *= 4.294967296e9;
		t->fraction = temp;
		t->int_part = htonl(t->int_part);
		t->fraction = htonl(t->fraction);
	} else {
		value = -value;
		t->int_part = value;
		temp = value - t->int_part;
		temp *= 4.294967296e9;
		t->fraction = temp;
		t->int_part = htonl(~t->int_part);
		t->fraction = htonl(~t->fraction);
	}
}

void
double_to_s_fixed(t, value)
	struct s_fixedpt *t;
	double value;
{
	double temp;

	if (value >= (double) 0.0) {
		t->int_part = value;
		temp = value - t->int_part;
		temp *= 65536.0;
		t->fraction = temp;
		t->int_part = htons(t->int_part);
		t->fraction = htons(t->fraction);
	} else {
		value = -value;
		t->int_part = value;
		temp = value - t->int_part;
		temp *= 65536.0;
		t->fraction = temp;
		t->int_part = htons(~t->int_part);
		t->fraction = htons(~t->fraction);
	}
}
/*
	in the sun, trying to assign a float between 2^31 and 2^32
	results in the value 2^31.  Neither 4.2bsd nor VMS have this
	problem.  Reported it to Bob O'Brien of SMI
*/
#ifdef	SUN_FLT_BUG
tstamp(stampp, tvp)
	struct l_fixedpt *stampp;
	struct timeval *tvp;
{
	int tt;
	double dd;

	stampp->int_part = ntohl(JAN_1970 + tvp->tv_sec);
	dd = (float) tvp->tv_usec / 1000000.0;
	tt = dd * 2147483648.0;
	stampp->fraction = ntohl((tt << 1));
}
#else
tstamp(stampp, tvp)
	struct l_fixedpt *stampp;
	struct timeval *tvp;
{
	stampp->int_part = ntohl((afs_uint32) (JAN_1970 + tvp->tv_sec));
	stampp->fraction = ntohl((afs_uint32) ((float) tvp->tv_usec * 4294.967295));
}
#endif

/*
 * ntoa is similar to inet_ntoa, but cycles through a set of 8 buffers
 * so it can be invoked several times in a function parameter list.
 */

char *
ntoa (in_addr)
struct in_addr in_addr;
{
	static int i = 0;
	static char bufs[8][20];
	unsigned char *p = (unsigned char *) &in_addr.s_addr;

	i = (i + 1) % (sizeof bufs / sizeof bufs[0]);
	sprintf (bufs[i], "%d.%d.%d.%d", p[0], p[1], p[2], p[3]);
	return bufs[i];
}

/* calculate effective precision, but repeated calls to gettimeofday */

int MeasurePrecision (intervalP)
  int *intervalP;
{
#if	defined(AFS_SUN5_ENV)
#define MAXTIMEDIFFS 100
#else
#define MAXTIMEDIFFS 10
#endif

    int diff[MAXTIMEDIFFS];
    int nDiff;
    int interval;
    int i;
    int q;
    struct timeval tv0;

    gettimeofday (&tv0, 0);
    nDiff = 0;
    while (nDiff < MAXTIMEDIFFS) {
	struct timeval tv, ntv;
	int counting = 2;		/* a counting kernel */
	gettimeofday (&tv, 0);
	do {
	    gettimeofday (&ntv, 0);

	    /*
	     * Bail if we are taking too long -- 30 seconds is arbitrary,
	     * but better than the previous approach, which was to bail
	     * after an arbitrary count of calls to gettimeofday().  This
	     * caused problems because the machines kept getting faster
	     * and the count kept getting exceeded.
	     */
	    if (ntv.tv_sec - tv0.tv_sec > 30) return 0;

	    interval = (ntv.tv_sec - tv.tv_sec)*1000000 +
		ntv.tv_usec - tv.tv_usec;
	    if (interval <= counting) counting = interval+2;
	} while (interval <= counting);	/* RT & sun4/280 kernels count */
	if (interval < 0) return 0;	/* shouldn't happen but who knows... */
	if (interval > 0) diff[nDiff++] = interval;
    }

    /* find average interval */
    interval = 0;
    for (i=0; i<MAXTIMEDIFFS; i++)
	interval += diff[i];
    interval = (interval + (MAXTIMEDIFFS/2)) / MAXTIMEDIFFS; /* round */
    if (interval == 0) return 0;	/* some problem... */
    if (intervalP) *intervalP = interval;

    /* calculate binary exponent of interval in seconds */
    q = 1000000;
    i = 0;
    while (q > interval) {
	q >>= 1;
	i--;
    }
    return i;
}
