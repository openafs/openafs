/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef __INCL_KTIME_
#define __INCL_KTIME_ 1

#undef min			/* redefined at end of file */
struct ktime_date {
    afs_int32 mask;		/* mask of valid fields */
    short year;
    short month;
    short day;
    short hour;
    short min;
    short sec;
};

/* could we make these bits the same? */

#define KTIMEDATE_YEAR		1
#define KTIMEDATE_MONTH		2
#define KTIMEDATE_DAY		4
#define KTIMEDATE_HOUR		8
#define KTIMEDATE_MIN		0x10
#define KTIMEDATE_SEC		0x20
#define KTIMEDATE_NEVER		0x1000	/* special case for never */
#define KTIMEDATE_NOW		0x2000	/* special case for now */

/* note that this if different from the value used by ParcePeriodic (sigh). */
#define KTIMEDATE_NEVERDATE	0xffffffff

struct ktime {
    int mask;
    short hour;			/* 0 - 23 */
    short min;			/* 0 - 60 */
    short sec;			/* 0 - 60 */
    short day;			/* 0 is sunday */
};

#define	KTIME_HOUR	1	/* hour should match */
#define	KTIME_MIN	2
#define	KTIME_SEC	4
#define	KTIME_TIME	7	/* all times should match */
#define	KTIME_DAY	8	/* day should match */
#define	KTIME_NEVER	0x10	/* special case: never */
#define	KTIME_NOW	0x20	/* special case: right now */

#define	ktime_DateToLong	ktime_DateToInt32	/* XXX */
#define	ktimeRelDate_ToLong	ktimeRelDate_ToInt32	/* XXX */
#define	LongTo_ktimeRelDate     Int32To_ktimeRelDate	/* XXX */

afs_int32 ktime_InterpretDate(struct ktime_date *akdate);

#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif /* __INCL_KTIME_ */
