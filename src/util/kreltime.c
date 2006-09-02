/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header: /cvs/openafs/src/util/kreltime.c,v 1.8.2.3 2006/07/31 21:12:59 shadow Exp $");

#include <afs/stds.h>
#include <sys/types.h>
#include <stdio.h>
#include "ktime.h"
#include <time.h>
#include <ctype.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif
#include "afsutil.h"


/* maximum values for relative dates */

#define	MAX_YEAR_VALUE	0
#define	MAX_MONTH_VALUE 11
#define	MAX_DAY_VALUE	30

/* for parsing relative expiration dates */
static struct parseseqS {
    afs_int32 ps_field;
    char ps_keychar;
    afs_int32 ps_maxValue;
} parseseq[] = {
    {
    KTIMEDATE_YEAR, 'y', MAX_YEAR_VALUE,},	/* no max. value */
    {
    KTIMEDATE_MONTH, 'm', MAX_MONTH_VALUE,},	/* months max. 12 */
    {
    KTIMEDATE_DAY, 'd', MAX_DAY_VALUE,},	/* days max. 31 */
    {
    0, 0, 0,}
};

/* Encodings to and from relative dates. The caller is responsible for
 * enforcing appropriate use of these routines
 */


/* ktimeRelDate_ToInt32
 *	converts a relative ktime date into an afs_int32.
 * exit:
 *	afs_int32 value of encoded date.
 */

afs_int32
ktimeRelDate_ToInt32(struct ktime_date *kdptr)
{
    afs_int32 retval;

    retval =
	(((kdptr->year * (MAX_MONTH_VALUE + 1)) +
	  kdptr->month) * (MAX_DAY_VALUE + 1)) + kdptr->day;
    return (retval);
}

/* Int32To_ktimeRelDate
 *	Convert a relative date encoded in an afs_int32 - back into a ktime_date
 *	structure
 */

int
Int32To_ktimeRelDate(afs_int32 int32Date, struct ktime_date *kdptr)
{
    memset(kdptr, 0, sizeof(*kdptr));

    kdptr->day = int32Date % (MAX_DAY_VALUE + 1);
    if (kdptr->day != 0)
	kdptr->mask |= KTIMEDATE_DAY;

    int32Date = int32Date / (MAX_DAY_VALUE + 1);

    kdptr->month = int32Date % (MAX_MONTH_VALUE + 1);
    if (kdptr->month != 0)
	kdptr->mask |= KTIMEDATE_MONTH;

    int32Date = int32Date / (MAX_MONTH_VALUE + 1);

    kdptr->year = int32Date;
    if (kdptr->year != 0)
	kdptr->mask |= KTIMEDATE_YEAR;

    return (0);
}

/* ktimeDate_FromInt32
 *	Converts a time in seconds, to a time (in ktime_date format).
 *	Result is a conventional ktime_date structure.
 *	placed in the supplied structure
 * entry:
 *	timeSecs - time in seconds
 *	ktimePtr - ptr to struct for the return value
 */

int
ktimeDate_FromInt32(afs_int32 timeSecs, struct ktime_date *ktimePtr)
{
    time_t     tt = timeSecs;
    struct tm *timePtr;
#ifndef AFS_NT40_ENV
    struct tm timeP;

    timePtr = &timeP;

    memset(&timePtr, 0, sizeof(timePtr));
    localtime_r(&tt, &timePtr);
#else
    timePtr = localtime(&tt);
#endif

    /* copy the relevant fields */
    ktimePtr->sec = timePtr->tm_sec;
    ktimePtr->min = timePtr->tm_min;
    ktimePtr->hour = timePtr->tm_hour;
    ktimePtr->day = timePtr->tm_mday;
    ktimePtr->month = timePtr->tm_mon + 1;
    ktimePtr->year = timePtr->tm_year;

    ktimePtr->mask =
	KTIMEDATE_YEAR | KTIMEDATE_MONTH | KTIMEDATE_DAY | KTIMEDATE_HOUR |
	KTIMEDATE_MIN | KTIMEDATE_SEC;

    return (0);
}

#define	RD_DIGIT_LIMIT	4	/* max. no. digits permitted */

/* ParseRelDate
 *	Parses a relative date of the form  <n>y<n>m<n>d representing years
 *	months and days. <n> is limited to RD_DIGIT_LIMIT digits in length
 *	and is further restricted by the maximums specified at the head
 *	of the file.
 * entry:
 *	dateStr - ptr to string to parse. Leading white space ingnored.
 * exit:
 *	returns a ptr to a static ktime_date structure containing
 *		appropriately set fields. The mask field is unused.
 *	0 - error in date specification
 */

afs_int32
ParseRelDate(char *dateStr, struct ktime_date * relDatePtr)
{
    struct parseseqS *psPtr;
    afs_int32 value, digit_limit;
    afs_int32 type_index;

    memset(relDatePtr, 0, sizeof(*relDatePtr));
    type_index = 0;

    while (1) {			/*w */

	while (isspace(*dateStr))	/* skip leading whitespace */
	    dateStr++;

	if (isdigit(*dateStr) == 0)
	    goto error;

	digit_limit = RD_DIGIT_LIMIT;
	value = 0;
	while (isdigit(*dateStr)) {
	    value = value * 10 + *dateStr - '0';
	    dateStr++;
	    if (digit_limit-- == 0)
		goto error;
	}

	psPtr = &parseseq[type_index];
	/* determine the units. Search for a matching type character */
	while ((psPtr->ps_keychar != *dateStr)
	       && (psPtr->ps_keychar != 0)
	    ) {
	    type_index++;
	    psPtr = &parseseq[type_index];
	}

	/* no matching type found */
	if (psPtr->ps_keychar == 0)
	    goto error;

	/* check the bounds on the maximum value. Can't be negative
	 * and if a maximum value is specified, check against it
	 */
	if ((value < 0)
	    || ((psPtr->ps_maxValue > 0) && (value > psPtr->ps_maxValue))
	    )
	    goto error;

	/* save computed value in the relevant type field */
	switch (psPtr->ps_field) {
	case KTIMEDATE_YEAR:
	    relDatePtr->year = value;
	    relDatePtr->mask |= KTIMEDATE_YEAR;
	    break;

	case KTIMEDATE_MONTH:
	    if (value > MAX_MONTH_VALUE)
		goto error;
	    relDatePtr->month = value;
	    relDatePtr->mask |= KTIMEDATE_MONTH;
	    break;

	case KTIMEDATE_DAY:
	    if (value > MAX_DAY_VALUE)
		goto error;

	    relDatePtr->mask |= KTIMEDATE_DAY;
	    relDatePtr->day = value;
	    break;

	default:
	    goto error;
	}
	dateStr++;		/* next digit */

	if (*dateStr == 0) {
	    /* no more chars to process, return the result */
	    return (0);
	}
    }				/*w */

  error:
    return (1);
}

/* RelDatetoString
 *	returns a static string representing the relative date. This is in
 *	a format acceptable to the relative date parser.
 * entry:
 *	datePtr - relative date to be converted.
 * exit:
 *	ptr to static string
 */

char *
RelDatetoString(struct ktime_date *datePtr)
{
    static char dateString[64];
    char tempstring[64], *sptr;

    dateString[0] = 0;
    sptr = &dateString[0];

    if (datePtr->mask & KTIMEDATE_YEAR) {
	sprintf(tempstring, "%-dy", datePtr->year);
	strcat(sptr, tempstring);
    }

    if (datePtr->mask & KTIMEDATE_MONTH) {
	strcat(sptr, " ");
	sprintf(tempstring, "%-dm", datePtr->month);
	strcat(sptr, tempstring);
    }

    if (datePtr->mask & KTIMEDATE_DAY) {
	strcat(sptr, " ");
	sprintf(tempstring, "%-dd", datePtr->day);
	strcat(sptr, tempstring);
    }
    return (sptr);
}

/* Add_RelDate_to_Time
 *	Returns current time with a relative time added. Note that the 
 *	computation adds in most significant fields first, i.e. year, month
 *	day etc. Addition of least significant fields would produce different
 *	results (depending on the data).
 * entry:
 *	relDatePtr - a ktime_date containing a relative time specification
 * exit:
 *	returns specified time with relative time added.
 */

afs_int32
Add_RelDate_to_Time(struct ktime_date * relDatePtr, afs_int32 atime)
{
    afs_int32 moreYears;
    static struct ktime_date absDate;

    ktimeDate_FromInt32(atime, &absDate);	/* convert to ktime */

    /* add in years */
    if (relDatePtr->mask & KTIMEDATE_YEAR)
	absDate.year += relDatePtr->year;

    /* add in months */
    if (relDatePtr->mask & KTIMEDATE_MONTH)
	absDate.month += relDatePtr->month;

    if (absDate.month > 12) {
	moreYears = absDate.month / 12;
	absDate.month = absDate.month % 12;
	absDate.year += moreYears;
    }

    /* day computations depend upon month size, so do these in seconds */
    atime = ktime_InterpretDate(&absDate);

    if (relDatePtr->mask & KTIMEDATE_DAY)
	atime = atime + relDatePtr->day * 24 * 60 * 60;

    if (relDatePtr->mask & KTIMEDATE_HOUR)
	atime = atime + relDatePtr->hour * 60 * 60;

    if (relDatePtr->mask & KTIMEDATE_MIN)
	atime = atime + relDatePtr->min * 60;

    if (relDatePtr->mask & KTIMEDATE_SEC)
	atime = atime + relDatePtr->sec;

    return (atime);
}
