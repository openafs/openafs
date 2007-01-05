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
    ("$Header$");

#include <afs/stds.h>
#include <sys/types.h>
#include <afs/ktime.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <afs/cmd.h>
#include <ctype.h>
#include "bc.h"


#define	MAX_YEAR_VALUE	0
#define	MAX_MONTH_VALUE 11
#define	MAX_DAY_VALUE	30

/* for parsing relative expiration dates */
static struct parseseqS {
    afs_int32 ps_field;
    char ps_keychar;
    afs_int32 ps_maxValue;
} parseseq[] = {
    KTIMEDATE_YEAR, 'y', MAX_YEAR_VALUE,	/* no max. value */
	KTIMEDATE_MONTH, 'm', MAX_MONTH_VALUE,	/* months max. 12 */
	KTIMEDATE_DAY, 'd', MAX_DAY_VALUE,	/* days max. 31 */
0, 0, 0,};

/* Encodings to and from relative dates. The caller is responsible for
 * enforcing appropriate use of these routines
 */
afs_int32
ktimeRelDate_ToLong(kdptr)
     struct ktime_date *kdptr;
{
    afs_int32 retval;

    retval =
	(((kdptr->year * (MAX_MONTH_VALUE + 1)) +
	  kdptr->month) * (MAX_DAY_VALUE + 1)) + kdptr->day;
    return (retval);
}

/* LongTo_ktimeRelDate
 *	Convert a relative date encoded in a long - back into a ktime_date
 *	structure
 */

LongTo_ktimeRelDate(longDate, kdptr)
     afs_int32 longDate;
     struct ktime_date *kdptr;
{
    memset(kdptr, 0, sizeof(*kdptr));

    kdptr->day = longDate % (MAX_DAY_VALUE + 1);
    if (kdptr->day != 0)
	kdptr->mask |= KTIMEDATE_DAY;

    longDate = longDate / (MAX_DAY_VALUE + 1);

    kdptr->month = longDate % (MAX_MONTH_VALUE + 1);
    if (kdptr->month != 0)
	kdptr->mask |= KTIMEDATE_MONTH;

    longDate = longDate / (MAX_MONTH_VALUE + 1);

    kdptr->year = longDate;
    if (kdptr->year != 0)
	kdptr->mask |= KTIMEDATE_YEAR;

    return (0);
}


#ifdef notdef
/* First some date storage and retrieval routines.
 * These routines are for convert ktime date structures into text, and back.
 * the format is compact and suitable for relative dates. These are used
 * to store dates into the backup database.
 */

/* ktimeDate_ToCompactString
 *	convert the ktime into a compact text form.
 * exit:
 *	pointer to a static string containing the text form of the ktime.
 * notes:
 *	it assumes that non-valid portions of the date are set to 0 and that
 *	all date fields are non-negative.
 */

char *
ktimeDate_ToCompactString(kdptr)
     struct ktime_date *kdptr;
{
    char buffer[1024];

    sprintf(buffer, "%-d:%-d:%-d:%-d:%-d:%-d", kdptr->year, kdptr->month,
	    kdptr->day, kdptr->hour, kdptr->min, kdptr->sec);

    return (&buffer[0]);
}

/* CompactStringTo_ktimeDate
 *	parses the string and places it in the ktime structure
 * exit:
 *	0 - success
 *	-1 - failed, string format error
 */
afs_int32
CompactStringTo_ktimeDate(stptr, kdptr)
     char *stptr;
     struct ktime_date *kdptr;
{
    afs_int32 code;

    code =
	sscanf(stptr, "%d:%d:%d:%d:%d:%d", &kdptr->year, &kdptr->month,
	       &kdptr->day, &kdptr->hour, &kdptr->min, &kdptr->sec);

    if (code != 6)
	return (-1);

    kdptr->mask = 0;

    if (kdptr->year)
	kdptr->mask |= KTIMEDATE_YEAR;

    if (kdptr->month)
	kdptr->mask |= KTIMEDATE_MONTH;

    if (kdptr->day)
	kdptr->mask |= KTIMEDATE_DAY;

    if (kdptr->hour)
	kdptr->mask |= KTIMEDATE_HOUR;

    if (kdptr->min)
	kdptr->mask |= KTIMEDATE_MIN;

    if (kdptr->sec)
	kdptr->mask |= KTIMEDATE_SEC;

    return (0);
}

/* ktimeDate_FromLong
 *	Converts a time in seconds, to a time (in ktime_date format). Result is
 *	placed in the supplied structure
 * entry:
 *	timeSecs - time in seconds
 *	ktimePtr - ptr to struct for the return value
 */

ktimeDate_FromLong(timeSecs, ktimePtr)
     afs_int32 timeSecs;
     struct ktime_date *ktimePtr;
{
    struct tm timePtr;

    timePtr = localtime(&timeSecs);

    /* copy the relevant fields */
    ktimePtr->sec = timePtr->sec;
    ktimePtr->min = timePtr->min;
    ktimePtr->hour = timePtr->hour;
    ktimePtr->day = timePtr->mday;
    ktimePtr->month = timePtr->mon;
    ktimePtr->year = timePtr->year;

    ktimePtr->mask =
	KTIMEDATE_YEAR | KTIMEDATE_MONTH | KTIMEDATE_DAY | KTIMEDATE_HOUR |
	KTIMEDATE_MIN | KTIMEDATE_SEC;

    return (0);
}


/* AddKtimeToNow
 *	Returns current time with a relative time added. Note that the 
 *	computation adds in most significant fields first, i.e. year, month
 *	day etc. Addition of least significant fields would produce different
 *	results (depending on the data).
 * entry:
 *	relDatePtr - a ktime_date containing a relative time specification
 * exit:
 *	returns current time with relative time added.
 */

afs_int32
AddKtimeToNow(relDatePtr)
     struct ktime_date *relDatePtr;
{
    struct parseseqS typePtr;
    afs_int32 curTime;
    afs_int32 moreYears;
    static struct ktime_date absDate;

    curTime = time(0);		/* get current time */
    ktimeDate_FromLong(curTime, &absDate);	/* convert to ktime */

    /* add in years */
    absDate.year += relDatePtr->year;

    /* add in months */
    absDate.month += relDatePtr->month;
    if (absDate.month > 12) {
	moreYears = absDate.month / 12;
	absDate.month = absDate.month % 12;
	absDate.year += moreYears;
    }

    /* day computations depend upon month size, so do these in seconds */
    curTime = ktime_InterpretDate(&absDate);

    curTime =
	curTime + relDatePtr->sec + relDatePtr->min * 60 +
	relDatePtr->hour * 60 * 60 + relDatePtr->day * 24 * 60 * 60;

    return (curTime);
}
#endif /* notdef */

#define	RD_DIGIT_LIMIT	4	/* max. no. digits permitted */


/* ParseRelDate
 *	Parses a relative date of the form  <n>y<n>m<n>d representing years
 *	months and days. <n> is limited to RD_DIGIT_LIMIT digits in length.
 * entry:
 *	dateStr - ptr to string to parse. Leading white space ingnored.
 * exit:
 *	returns a ptr to a static ktime_date structure containing
 *		appropriately set fields. The mask field is unused.
 *	0 - error in date specification
 */

afs_int32
ParseRelDate(dateStr, relDatePtr)
     char *dateStr;
     struct ktime_date *relDatePtr;
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
	    relDatePtr->month = value;
	    relDatePtr->mask |= KTIMEDATE_MONTH;
	    break;

	case KTIMEDATE_DAY:
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
RelDatetoString(datePtr)
     struct ktime_date *datePtr;
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

#define	FAIL(n)						\
{							\
     code = n;						\
     goto error;					\
}

/* bc_ParseExpiration
 *
 * Notes:
 *	dates are specified as absolute or relative, the syntax is:
 *	absolute:	at %d/%d/%d [%d:%d]	where [..] is optional
 *	relative:	in [%dy][%dm][%dd]	where at least one component
 *						must be specified
 */

afs_int32
bc_ParseExpiration(paramPtr, expType, expDate)
     struct cmd_parmdesc *paramPtr;
     afs_int32 *expType;
     afs_int32 *expDate;
{
    struct cmd_item *itemPtr, *tempPtr;
    struct ktime_date kt;
    char *dateString;
    afs_int32 length = 0;
    afs_int32 code = 0;

    if (paramPtr->items == 0) {
	/* no expiration specified */
	*expType = BC_NO_EXPDATE;
	*expDate = 0;
	return (0);
    }

    /* some form of expiration date specified. First validate the prefix */
    itemPtr = paramPtr->items;

    if (strcmp(itemPtr->data, "at") == 0) {
	*expType = BC_ABS_EXPDATE;
    } else if (strcmp(itemPtr->data, "in") == 0) {
	*expType = BC_REL_EXPDATE;
    } else
	FAIL(1);

    /* before parsing the date string - concatenate all the pieces */
    itemPtr = itemPtr->next;
    tempPtr = itemPtr;

    /* compute the length of string required */
    while (tempPtr != 0) {
	length += strlen(tempPtr->data);
	tempPtr = tempPtr->next;
	length++;		/* space or null terminator */
    }
    if (length == 0)		/* no actual date string */
	FAIL(1);

    dateString = (char *)malloc(length);
    if (dateString == 0)
	FAIL(2);

    /* now assemble the date string */
    dateString[0] = 0;
    while (itemPtr != 0) {
	strcat(dateString, itemPtr->data);
	itemPtr = itemPtr->next;
	if (itemPtr != 0)
	    strcat(dateString, " ");
    }

    switch (*expType) {
    case BC_ABS_EXPDATE:
	code = ktime_DateToLong(dateString, expDate);
	if (code)
	    FAIL(1);
	break;

    case BC_REL_EXPDATE:
	code = ParseRelDate(dateString, &kt);
	if (code)
	    FAIL(1);
	*expDate = ktimeRelDate_ToLong(&kt);
	break;

    default:
	FAIL(1);
    }

    /* code will be 0 */
  exit:
    /* normal exit */
    if (dateString)
	free(dateString);
    return (code);

  error:
    /* assign default values */
    *expType = BC_NO_EXPDATE;
    *expDate = 0;
    goto exit;
}
