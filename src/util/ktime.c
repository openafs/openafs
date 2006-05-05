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

#include <sys/types.h>
#include <stdio.h>
#include <time.h>
#include <ctype.h>
#ifdef AFS_NT40_ENV
#include <malloc.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif
#include <stdlib.h>
#include "afsutil.h"

#include "ktime.h"

/* some date parsing routines */

struct token {
    struct token *next;
    char *key;
};

static char *day[] = {
    "sun",
    "mon",
    "tue",
    "wed",
    "thu",
    "fri",
    "sat"
};

/* free token list returned by parseLine */
#ifdef undef
static
LocalFreeTokens(alist)
     register struct token *alist;
{
    register struct token *nlist;
    for (; alist; alist = nlist) {
	nlist = alist->next;
	free(alist->key);
	free(alist);
    }
    return 0;
}
#endif

static int
space(int x)
{
    if (x == 0 || x == ' ' || x == '\t' || x == '\n')
	return 1;
    else
	return 0;
}

static int
LocalParseLine(char *aline, struct token **alist)
{
    char tbuffer[256];
    register char *tptr = NULL;
    int inToken;
    struct token *first, *last;
    register struct token *ttok;
    register int tc;

    inToken = 0;		/* not copying token chars at start */
    first = NULL;
    last = NULL;
    while (1) {
	tc = *aline++;
	if (tc == 0 || space(tc)) {	/* terminating null gets us in here, too */
	    if (inToken) {
		inToken = 0;	/* end of this token */
		*tptr++ = 0;
		ttok = (struct token *)malloc(sizeof(struct token));
		ttok->next = NULL;
		ttok->key = (char *)malloc(strlen(tbuffer) + 1);
		strcpy(ttok->key, tbuffer);
		if (last) {
		    last->next = ttok;
		    last = ttok;
		} else
		    last = ttok;
		if (!first)
		    first = ttok;
	    }
	} else {
	    /* an alpha character */
	    if (!inToken) {
		tptr = tbuffer;
		inToken = 1;
	    }
	    if (tptr - tbuffer >= sizeof(tbuffer))
		return -1;	/* token too long */
	    *tptr++ = tc;
	}
	if (tc == 0) {
	    /* last token flushed 'cause space(0) --> true */
	    if (last)
		last->next = NULL;
	    *alist = first;
	    return 0;
	}
    }
}

/* keyword database for periodic date parsing */
static struct ptemp {
    char *key;
    afs_int32 value;
} ptkeys[] = {
    {
    "sun", 0x10000,}, {
    "mon", 0x10001,}, {
    "tue", 0x10002,}, {
    "wed", 0x10003,}, {
    "thu", 0x10004,}, {
    "fri", 0x10005,}, {
    "sat", 0x10006,}, {
    "sunday", 0x10000,}, {
    "monday", 0x10001,}, {
    "tuesday", 0x10002,}, {
    "wednesday", 0x10003,}, {
    "thursday", 0x10004,}, {
    "thur", 0x10004,}, {
    "friday", 0x10005,}, {
    "saturday", 0x10006,}, {
    "am", 0x20000,}, {
    "pm", 0x20001,}, {
    "a.m.", 0x20000,}, {
    "p.m.", 0x20001,}, {
    0, 0,}
};

/* ktime_DateOf
 * entry:
 *	atime - time in seconds (Unix std)
 * exit:
 *	return value - ptr to time in text form. Ptr is to a static string.
 */

char *
ktime_DateOf(afs_int32 atime)
{
    static char tbuffer[30];
    register char *tp;
    time_t t = atime;
    tp = ctime(&t);
    if (tp) {
	strcpy(tbuffer, tp);
	tbuffer[24] = 0;	/* get rid of new line */
    } else
	strcpy(tbuffer, "BAD TIME");
    return tbuffer;
}

/* ParseTime
 *	parse 12:33:12 or 12:33 or 12 into ktime structure
 * entry:
 *	astr - ptr to string to be parsed
 *	ak - ptr to struct for return value.
 * exit:
 *	0 - ak holds parsed time.
 *	-1 - error in format
 */

static int
ParseTime(register struct ktime *ak, register char *astr)
{
    int field;
    afs_int32 temp;
    register char *tp;
    register int tc;

    field = 0;			/* 0=hour, 1=min, 2=sec */
    temp = 0;

    ak->mask |= (KTIME_HOUR | KTIME_MIN | KTIME_SEC);
    for (tp = astr;;) {
	tc = *tp++;
	if (tc == 0 || tc == ':') {
	    if (field == 0)
		ak->hour = temp;
	    else if (field == 1)
		ak->min = temp;
	    else if (field == 2)
		ak->sec = temp;
	    temp = 0;
	    field++;
	    if (tc == 0)
		break;
	    continue;
	} else if (!isdigit(tc))
	    return -1;		/* syntax error */
	else {
	    /* digit */
	    temp *= 10;
	    temp += tc - '0';
	}
    }
    if (ak->hour >= 24 || ak->min >= 60 || ak->sec >= 60)
	return -1;
    return 0;
}

afs_int32
ktime_Str2int32(register char *astr)
{
    struct ktime tk;

    memset(&tk, 0, sizeof(tk));
    if (ParseTime(&tk, astr))
	return (-1);		/* syntax error */

    return ((tk.hour * 60 + tk.min) * 60 + tk.sec);
}

/* ktime_ParsePeriodic
 *	Parses periodic date of form
 *		now | never | at [time spec] | every [time spec]
 *	where [time spec] is a ktime string.
 * entry:
 *	adate - string to be parsed
 *	ak - ptr to structure for returned ktime
 * exit:
 *	0 - parsed ktime in ak
 *	-1 - specification error
 */

/* -1 means error, 0 means now, otherwise returns time of next event */
int
ktime_ParsePeriodic(char *adate, register struct ktime *ak)
{
    struct token *tt;
    register afs_int32 code;
    struct ptemp *tp;

    memset(ak, 0, sizeof(*ak));
    code = LocalParseLine(adate, &tt);
    if (code)
	return -1;
    for (; tt; tt = tt->next) {
	/* look at each token */
	if (strcmp(tt->key, "now") == 0) {
	    ak->mask |= KTIME_NOW;
	    return 0;
	}
	if (strcmp(tt->key, "never") == 0) {
	    ak->mask |= KTIME_NEVER;
	    return 0;
	}
	if (strcmp(tt->key, "at") == 0)
	    continue;
	if (strcmp(tt->key, "every") == 0)
	    continue;
	if (isdigit(tt->key[0])) {
	    /* parse a time */
	    code = ParseTime(ak, tt->key);
	    if (code)
		return -1;
	    continue;
	}
	/* otherwise use keyword table */
	for (tp = ptkeys;; tp++) {
	    if (tp->key == NULL) {
		return -1;
	    }
	    if (strcmp(tp->key, tt->key) == 0)
		break;
	}
	/* now look at tp->value to see what we've got */
	if ((tp->value >> 16) == 1) {
	    /* a day */
	    ak->mask |= KTIME_DAY;
	    ak->day = tp->value & 0xff;
	}
	if ((tp->value >> 16) == 2) {
	    /* am or pm token */
	    if ((tp->value & 0xff) == 1) {
		/* pm */
		if (!(ak->mask & KTIME_HOUR))
		    return -1;
		if (ak->hour < 12)
		    ak->hour += 12;
		/* 12 is 12 PM */
		else if (ak->hour != 12)
		    return -1;
	    } else {
		/* am is almost a noop, except that we map 12:01 am to 0:01 */
		if (ak->hour > 12)
		    return -1;
		if (ak->hour == 12)
		    ak->hour = 0;
	    }
	}
    }
    return 0;
}

/* ktime_DisplayString
 *	Display ktime structure as English that could go into the ktime	parser
 * entry:
 *	aparm - ktime to be converted to string
 *	astring - ptr to string, for the result
 * exit:
 *	0 - astring contains ktime string.
 */
int
ktime_DisplayString(struct ktime *aparm, register char *astring)
{
    char tempString[50];

    if (aparm->mask & KTIME_NEVER) {
	strcpy(astring, "never");
	return 0;
    } else if (aparm->mask & KTIME_NOW) {
	strcpy(astring, "now");
	return 0;
    } else {
	strcpy(astring, "at");
	if (aparm->mask & KTIME_DAY) {
	    strcat(astring, " ");
	    strcat(astring, day[aparm->day]);
	}
	if (aparm->mask & KTIME_HOUR) {
	    if (aparm->hour > 12)
		sprintf(tempString, " %d", aparm->hour - 12);
	    else if (aparm->hour == 0)
		strcpy(tempString, " 12");
	    else
		sprintf(tempString, " %d", aparm->hour);
	    strcat(astring, tempString);
	}
	if (aparm->mask & KTIME_MIN) {
	    sprintf(tempString, ":%02d", aparm->min);
	    strcat(astring, tempString);
	}
	if ((aparm->mask & KTIME_SEC) && aparm->sec != 0) {
	    sprintf(tempString, ":%02d", aparm->sec);
	    strcat(astring, tempString);
	}
	if (aparm->mask & KTIME_HOUR) {
	    if (aparm->hour >= 12)
		strcat(astring, " pm");
	    else
		strcat(astring, " am");
	}
    }
    return 0;
}

/* get next time that matches ktime structure after time afrom */
afs_int32
ktime_next(struct ktime * aktime, afs_int32 afrom)
{
    /* try by guessing from now */
    struct tm *tsp;
    time_t start;		/* time to start looking */
    time_t probe;		/* a placeholder to use for advancing day to day */
    time_t time_next;		/* actual UTC time of probe, with time of day set */
    afs_int32 tmask;
    struct ktime_date tdate;

    start = afrom + time(0);	/* time to start search */
    tmask = aktime->mask;

    /* handle some special cases */
    if (tmask & KTIME_NEVER)
	return 0x7fffffff;
    if (tmask & KTIME_NOW)
	return 0;

    /* Use probe to fill in members of *tsp. Add 23 hours each iteration until 
     * time_next is correct. Only add 23 hrs to avoid skipping spring 
     * daylight savings time day */
    for (probe = start;; probe += (23 * 3600)) {
	tsp = localtime(&probe);	/* find out what UTC time "probe" is */

	tdate.year = tsp->tm_year;
	tdate.month = tsp->tm_mon + 1;
	tdate.day = tsp->tm_mday;
	tdate.mask =
	    KTIMEDATE_YEAR | KTIMEDATE_MONTH | KTIMEDATE_DAY | KTIMEDATE_HOUR
	    | KTIMEDATE_MIN | KTIMEDATE_SEC;
	tdate.hour = aktime->hour;	/* edit in our changes */
	tdate.min = aktime->min;
	tdate.sec = aktime->sec;
	time_next = ktime_InterpretDate(&tdate);	/* Convert back to UTC time */
	if (time_next < start)
	    continue;		/* "probe" time is already past */
	if ((tmask & KTIME_DAY) == 0)	/* don't care about day, we're done */
	    break;
	tsp = localtime(&time_next);
	if (tsp->tm_wday == aktime->day)
	    break;		/* day matches, we're done */
    }
    return time_next;
}


/* compare date in both formats, and return as in strcmp */
#ifdef undef
static int
KTimeCmp(register struct ktime *aktime, register struct tm *atm)
{
    register afs_int32 tmask;

    /* don't compare day of the week, since we can't tell the
     * order in a cyclical set.  Caller must check for equality, if
     * she cares */
    tmask = aktime->mask;
    if (tmask & KTIME_HOUR) {
	if (aktime->hour > atm->tm_hour)
	    return 1;
	if (aktime->hour < atm->tm_hour)
	    return -1;
    }
    if (tmask & KTIME_MIN) {
	if (aktime->min > atm->tm_min)
	    return 1;
	if (aktime->min < atm->tm_min)
	    return -1;
    }
    if (tmask & KTIME_SEC) {
	if (aktime->sec > atm->tm_sec)
	    return 1;
	if (aktime->sec < atm->tm_sec)
	    return -1;
    }
    return 0;
}
#endif

/* compare date in both formats, and return as in strcmp */
static int
KDateCmp(register struct ktime_date *akdate, register struct tm *atm)
{
    if (akdate->year > atm->tm_year)
	return 1;
    if (akdate->year < atm->tm_year)
	return -1;
    if (akdate->month > atm->tm_mon)
	return 1;
    if (akdate->month < atm->tm_mon)
	return -1;
    if (akdate->day > atm->tm_mday)
	return 1;
    if (akdate->day < atm->tm_mday)
	return -1;
    if (akdate->mask & KTIMEDATE_HOUR) {
	if (akdate->hour > atm->tm_hour)
	    return 1;
	if (akdate->hour < atm->tm_hour)
	    return -1;
    }
    if (akdate->mask & KTIMEDATE_MIN) {
	if (akdate->min > atm->tm_min)
	    return 1;
	if (akdate->min < atm->tm_min)
	    return -1;
    }
    if (akdate->mask & KTIMEDATE_SEC) {
	if (akdate->sec > atm->tm_sec)
	    return 1;
	if (akdate->sec < atm->tm_sec)
	    return -1;
    }
    return 0;
}

/* ktime_ParseDate
 * 	parse date string into ktime_date structure
 * entry:
 *	adate - string to be parsed
 *	akdate - ptr to ktime_date for result
 * exit:
 *	0 - akdate contains converted date
 *	-1 - parsing failure
 */

static afs_int32
ktime_ParseDate(char *adate, struct ktime_date *akdate)
{
    int code;
    afs_int32 month, day2, year, hour, min, sec;
    char never[7];
    char c[2];

    lcstring(never, adate, sizeof(never));
    if (strcmp(never, "never") == 0)
	akdate->mask = KTIMEDATE_NEVER;
    else if (strcmp(never, "now") == 0)
	akdate->mask = KTIMEDATE_NOW;
    else
	akdate->mask = 0;
    if (akdate->mask)
	return 0;

    /* Old ambiguous mm/dd/yy hh:mm:ss format */

    code =
	sscanf(adate, "%d / %d / %d %d : %d : %d%1s", &month, &day2, &year,
	       &hour, &min, &sec, &c[0]);
    if (code != 6) {
	sec = 0;
	code =
	    sscanf(adate, "%d / %d / %d %d : %d%1s", &month, &day2, &year,
		   &hour, &min, &c[0]);
	if (code != 5) {
	    hour = min = 0;
	    code =
		sscanf(adate, "%d / %d / %d%1s", &month, &day2, &year, &c[0]);
	    if (code != 3) {
		code = -1;
	    }
	}
    }

    /* New ISO 8601 (subset) format */

    if (code < 0) {
	hour = min = sec = 0;
	code =
	    sscanf(adate, "%d-%d-%d %d:%d:%d%1s", &year, &month, &day2,
		   &hour, &min, &sec, c);
	if (code != 3 && code != 5 && code != 6)
	    code = -1;
    }

    if (code < 0)
	return code;

    if ((year < 0) || (month < 1) || (month > 12) || (day2 < 1) || (day2 > 31) ||	/* more or less */
	(hour < 0) || (hour > 23) || (min < 0) || (min > 59) || (sec < 0)
	|| (sec > 59))
	return -2;

    if (year < 69)
	year += 100;		/* make 1/1/1 => Jan 1, 2001 */
    else if (year >= 1900)
	year -= 1900;		/* allow 1/1/2001 to work */
    else if (year > 99)
	return -2;		/* only allow 2 or 4 digit years */

    akdate->mask =
	KTIMEDATE_YEAR | KTIMEDATE_MONTH | KTIMEDATE_DAY | KTIMEDATE_HOUR |
	KTIMEDATE_MIN | KTIMEDATE_SEC;

    akdate->year = year;
    akdate->month = month;
    akdate->day = day2;
    akdate->hour = hour;
    akdate->min = min;
    akdate->sec = sec;

    /* done successfully */
    return 0;
}

/* ktime_DateToInt32
 *	Converts a ktime date string into an afs_int32
 * entry:
 *	adate - ktime date string
 *	aint32 - ptr to afs_int32
 * exit:
 *	0 - aint32 contains converted date.
 */

afs_int32
ktime_DateToInt32(char *adate, afs_int32 * aint32)
{
    struct ktime_date tdate;
    afs_int32 code;
    unsigned long l;
    char c[2];

    if (sscanf(adate, "%lu%1s", &l, c) == 1 && l > 200000000)
	*aint32 = l;
    else {
	/* parse the date into a ktime_date structure */
	code = ktime_ParseDate(adate, &tdate);
	if (code)
	    return code;		/* failed to parse */
	*aint32 = ktime_InterpretDate(&tdate);	/* interpret as seconds since 1970 */
    }

    return 0;
}

/* get useful error message to print about date input format */
char *
ktime_GetDateUsage(void)
{
    return "date format is '(yyyy-mm-dd | mm/dd/yy) [hh:mm]', using a 24 hour clock";
}


/* ktime_InterpretDate
 *	Converts ktime_date to an afs_int32
 * entry:
 *	akdate - date to be converted/interpreted
 * exit:
 *	returns KTIMEDATE_NEVERDATE - if never flag was set, or
 *	date converted to afs_int32.
 */

afs_int32
ktime_InterpretDate(struct ktime_date * akdate)
{
    register afs_uint32 tresult;
    register afs_uint32 tbit;
    time_t temp;
    register struct tm *tsp;

    if (akdate->mask & KTIMEDATE_NOW)
	return time(0);
    if (akdate->mask & KTIMEDATE_NEVER)
	return KTIMEDATE_NEVERDATE;

    tbit = 1 << 30;		/* start off at max signed result */
    tresult = 0;		/* result to return */
    while (tbit > 0) {
	temp = tresult + tbit;	/* see if adding this bit keeps us < akdate */
	tsp = localtime(&temp);
	tsp->tm_mon++;
#ifdef notdef
	if (tsp->tm_mon == 0) {
	    tsp->tm_mon = 12;
	    tsp->tm_year--;
	}
#endif
	if (KDateCmp(akdate, tsp) >= 0) {
	    /* if temp still represents earlier than date than we're searching
	     * for, add in bit as increment, otherwise use old value and look
	     * for smaller increment */
	    tresult = temp;
	}
	tbit = tbit >> 1;	/* try next bit */
    }

    return tresult;
}
